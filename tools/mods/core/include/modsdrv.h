/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file modsdrv.h
 * @brief MODS Device Driver interface
 *
 * MODS Device Driver (ModsDrv) interface -- a C API that other drivers that use
 * MODS can call into.
 *
 * A note on doxygenized comments: if you see a directive with @, that is
 * directed at the doxygen parser and can be ignored by anyone reading this
 * file directly.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "irqparams.h"
#include "lwguid.h"
#include "types.h"
#include "modsdrv_defines.h"

#include <stddef.h>
#include <stdarg.h>

#if defined(__GNUC__) && (__GNUC__ >= 4)
#define MODS_EXPORT __attribute__((visibility("default")))
#else
#define MODS_EXPORT
#endif

#if defined(BUILDING_MODS_LIB)
#define MODS_EMPTY_BODY { }
#define MODS_RETURN(val) { return (val); }
#else
#define MODS_EMPTY_BODY ;
#define MODS_RETURN(val) ;
#endif

#define MODSDRV_UNSPECIFIED_GPUINST  (~0U)

//------------------------------------------------------------------------------
// This function is used by OpenGL driver to load the rtcore & spriv libraries.
// Note these defines must match the LoadDLLFlags in the Xp namespace
#define LW_RTLD_NONE            0x00
#define LW_RTLD_UNLOAD_ON_EXIT  0x01
#define LW_RTLD_GLOBAL          0x02
#define LW_RTLD_DEEPBIND        0x04
#define LW_RTLD_LAZY            0x08
#define LW_RTLD_NODELETE        0x10

MODS_EXPORT void * ModsDrvLoadLibrary(const char* filename, int flags) MODS_RETURN(0)

MODS_EXPORT void ModsDrvLwGetExportTable(const void **ppExportTable,
                                         const void *pExportTableId) MODS_EMPTY_BODY
//! Interpret MODS_RC return values (prints the error message associated with
//! the given RC value)
MODS_EXPORT void   ModsDrvInterpretModsRC(MODS_RC rc) MODS_EMPTY_BODY

//------------------------------------------------------------------------------
// Debugging Functions
//------------------------------------------------------------------------------

/**
 * Break into the debugger when MODS is compiled as debug.
 * In both debug AND release mode there's a script option to enter interactive
 * (JavaScript interpretter) mode when a breakpoint is hit.
 *
 * At the top of your script file:
 * @code
 *
 * InteractOnBreakPoint = true;
 *
 * @endcode
 */
MODS_EXPORT void   ModsDrvBreakPoint(const char *file, int line) MODS_EMPTY_BODY
MODS_EXPORT void   ModsDrvBreakPointWithReason(int reason,
                                               const char *file,
                                               int line) MODS_EMPTY_BODY

// Error Info structs and relevant data types
typedef enum
{
    MODSDRV_NOT_AN_ERROR = 0,
    // Init error sub-codes
    MODSDRV_INITPHY_ERROR,
    MODSDRV_RXCAL_ERROR,
    MODSDRV_INITDLPL_ERROR,
    MODSDRV_INITLANEENABLE_ERROR,
    MODSDRV_SAFE_TRANSITION_ERROR,
    // Minion error sub-codes
    MODSDRV_MINION_BOOT_ERROR,
    MODSDRV_DLCMD_TIMEOUT_ERROR,
    // Clock error sub-codes
    MODSDRV_INITPLL_ERROR,
    MODSDRV_TXCLKSWITCH_PLL_ERROR,
    // Training error sub-codes
    MODSDRV_INITOPTIMIZE_ERROR,
    MODSDRV_ACTIVE_TRANSITION_ERROR,
    MODSDRV_INITTL_ERROR,
    MODSDRV_TL_TRANSITION_ERROR,
    // Configuration error sub-codes
    MODSDRV_INITPHASE1_ERROR,
    MODSDRV_INITNEGOTIATE_ERROR,
    MODSDRV_INITPHASE5_ERROR,
    //Shutown Error
    MODSDRV_LANESHUTDOWN_ERROR
} ModsDrvLwlinkErrorSubCode;

typedef struct
{
    UINT64 lwlinkMask;
    ModsDrvLwlinkErrorSubCode subCode;
} ModsDrvLwlinkErrorInfo;

typedef enum
{
    MODSDRV_LWLINK_ERROR_TYPE
} ModsDrvErrorInfoType;

// Generic ErrorInfo struct
typedef struct ModsDrvErrorInfo_tag
{
    ModsDrvErrorInfoType type;
    union
    {
        ModsDrvLwlinkErrorInfo lwlinkInfo;
    };
} ModsDrvErrorInfo;

MODS_EXPORT void ModsDrvBreakPointWithErrorInfo(int errorCategory,
                                                const char *file,
                                                int line,
                                                ModsDrvErrorInfo *pErrorInfo) MODS_EMPTY_BODY

/**
 * A note on the MODS print mechansim.
 *
 * Functions such as ModsDrvPrintf take a priority and print to stdout, a log
 * file (mods.log by default), serial port, and a cirlwlar buffer in memory.
 * By default, prints of PRI_NORMAL and above will print.
 *
 * Passing -verbose to most scripts will cause >= PRI_LOW prints to be output.
 *
 * See MODS documentation for further details on arguments to MODS such as
 *
 * -d, -C, and -S #
 *
 * that affect how/where/when prints are output.
 */

//! Print a string using the MODS print mechanism
MODS_EXPORT void   ModsDrvPrintString(int Priority, const char * String) MODS_EMPTY_BODY

//! Print a string using the MODS print mechanism (safe for ISRs)
MODS_EXPORT void   ModsDrvIsrPrintString(int Priority, const char * String) MODS_EMPTY_BODY

//! Flush the log file to disk
MODS_EXPORT void   ModsDrvFlushLogFile(void) MODS_EMPTY_BODY

//! printf using the MODS print mechanism
MODS_EXPORT int    ModsDrvPrintf(int Priority, const char *Fmt, ...)
#ifdef __GNUC__
    __attribute__((format (printf, 2, 3))) // GCC checks printf format string
#endif
MODS_RETURN(0)

//! vprintf using the MODS print mechanism
MODS_EXPORT int    ModsDrvVPrintf(int Priority, const char *Fmt, va_list Args) MODS_RETURN(0)

//! Set GPU id for the current thread, used during printing.
//! -1 indicates no current GPU.
//! Returns previous GPU id set on this thread.
MODS_EXPORT int    ModsDrvSetGpuId(int gpuInst) MODS_RETURN(0)

//! Set LwSwitch id for the current thread, used during printing.
//! -1 indicates no current GPU.
//! Returns previous GPU id set on this thread.
MODS_EXPORT UINT32 ModsDrvSetSwitchId(UINT32 switchInst) MODS_RETURN(0)

//------------------------------------------------------------------------------
// HW Error Logging
//------------------------------------------------------------------------------

/**
 * Keep track of HW errors that oclwrred and compare them against a list
 * of "expected" errors (useful for tests that intentionally cause HW errors)
 *
 * This API call is for drivers to alert MODS tests that an error oclwrred.  It
 * is not lwrrently compatible with "Type 2" External Tests (see Type 2
 * External Test section below)
 */
MODS_EXPORT void   ModsDrvLogError(const char *String) MODS_EMPTY_BODY

/**
 * Keep track of HW info that oclwrred and compare them against
 * a list of "expected" info (useful for tests that
 * intentionally cause HW errors)
 */
MODS_EXPORT void   ModsDrvLogInfo(const char *String) MODS_EMPTY_BODY

/**
 * Stop logging the current log entry.
 *
 * The logging facility allows log strings to be broken up
 * across multiple calls to ModsDrvLogError/ModsDrvLogInfo, this
 * API call will terminate logging of the current entry if a
 * current entry is in progress.
 */
MODS_EXPORT void   ModsDrvFlushLog(void) MODS_EMPTY_BODY

//------------------------------------------------------------------------------
// Mods Pooling memory allocator functions
//------------------------------------------------------------------------------

/**
 * A word on MODS memory allocation/free functions:
 *
 * MODS keeps a pool of memory which it manages for clients that use the API
 * calls below.  In debug mode it does all sorts of nice things such as
 * detecting certain kinds of memory overruns, memory leaks, etc.
 */

//! Use ModsDrvAlloc instead of calling malloc directly
MODS_EXPORT void * ModsDrvAlloc(size_t nbytes) MODS_RETURN(0)

//! Use ModsDrvCalloc instead of calling calloc directly
MODS_EXPORT void * ModsDrvCalloc(size_t numElements, size_t nbytes) MODS_RETURN(0)

//! Use ModsDrvRealloc instead of calling realloc directly
MODS_EXPORT void * ModsDrvRealloc(void * pOld, size_t nbytes) MODS_RETURN(0)

//! Anything that was allocated using ModsDrvAlloc/Calloc/Realloc must be freed
//! using ModsDrvFree.
MODS_EXPORT void   ModsDrvFree(void * p) MODS_EMPTY_BODY

//! Allocated memory with an alignment restriction.
MODS_EXPORT void * ModsDrvAlignedMalloc(UINT32 ByteSize, UINT32 AlignSize,
                                        UINT32 Offset) MODS_RETURN(0)

//! Free memory allocated with ModsDrvAlignedMalloc
MODS_EXPORT void   ModsDrvAlignedFree(void * Addr) MODS_EMPTY_BODY

//! Bypass the MODS memory pool, but still keep track of this allocation (ie
//! make sure that it gets freed)
MODS_EXPORT void * ModsDrvExecMalloc(size_t NumBytes) MODS_RETURN(0)

//! Free memory allocated by ModsDrvExecMalloc
MODS_EXPORT void   ModsDrvExecFree(void * Address, size_t NumBytes) MODS_EMPTY_BODY

//------------------------------------------------------------------------------
// Tasker functions
//------------------------------------------------------------------------------

/**
 * MODS uses a cooperative tasker (for repeatability reasons).  Obviously this
 * means that busy-waiting is not allowed.  Polling loops should periodically
 * call ModsDrvYield.
 */

//! Create a new thread
MODS_EXPORT UINT32 ModsDrvCreateThread(void (*ThreadFuncPtr)(void*),
                                       void* data, UINT32 StackSize,
                                       const char* Name) MODS_RETURN(0)

//! Join thread given timeout
MODS_EXPORT void ModsDrvJoinThread(UINT32 ThreadID) MODS_EMPTY_BODY

//! Self explanatory
MODS_EXPORT UINT32 ModsDrvGetLwrrentThreadId(void) MODS_RETURN(0)

//! Exit from the current thread
MODS_EXPORT void ModsDrvExitThread(void) MODS_EMPTY_BODY

//! Yield to another thread (if applicable)
MODS_EXPORT int    ModsDrvYield(void) MODS_RETURN(0)

//! Allocate thread local storage
MODS_EXPORT MODS_BOOL ModsDrvTlsAlloc(unsigned long * idx) MODS_RETURN(MODS_FALSE)

//! Free thread local storage
MODS_EXPORT void   ModsDrvTlsFree(unsigned long idx) MODS_EMPTY_BODY

//! Get the thread local storage at the given index
MODS_EXPORT void * ModsDrvTlsGet(unsigned long tlsIndex) MODS_RETURN(0)

//! Set the thread local storage at the given index
MODS_EXPORT void   ModsDrvTlsSet(unsigned long tlsIndex, void * value) MODS_EMPTY_BODY

//! Self explanatory
MODS_EXPORT void * ModsDrvAllocMutex(void) MODS_RETURN(0)

//! Free a mutex allocated with ModsDrvAllocMutex
MODS_EXPORT void   ModsDrvFreeMutex(void *hMutex) MODS_EMPTY_BODY

//! Self explanatory
MODS_EXPORT void   ModsDrvAcquireMutex(void *hMutex) MODS_EMPTY_BODY

//! Self explanatory
MODS_EXPORT MODS_BOOL ModsDrvTryAcquireMutex(void *hMutex) MODS_RETURN(MODS_FALSE)

//! Self explanatory
MODS_EXPORT void   ModsDrvReleaseMutex(void *hMutex) MODS_EMPTY_BODY

//! Self explanatory
MODS_EXPORT MODS_BOOL ModsDrvIsMutexOwner(void *hMutex) MODS_RETURN(MODS_FALSE)

//! Self explanatory
MODS_EXPORT void * ModsDrvAllocCondition(void) MODS_RETURN(0)

//! Self explanatory
MODS_EXPORT void ModsDrvFreeCondition(void *hCond) MODS_EMPTY_BODY

//! Self explanatory
MODS_EXPORT MODS_BOOL ModsDrvWaitCondition(void *hCond, void *hMutex) MODS_RETURN(MODS_FALSE)

//! Self explanatory
MODS_EXPORT MODS_BOOL ModsDrvWaitConditionTimeout(void *hCond, void *hMutex, FLOAT64 timeoutMs) MODS_RETURN(MODS_FALSE)

//! Self explanatory
MODS_EXPORT void ModsDrvSignalCondition(void *hCond) MODS_EMPTY_BODY

//! Self explanatory
MODS_EXPORT void ModsDrvBroadcastCondition(void *hCond) MODS_EMPTY_BODY

//! allocate event that has Manual reset
MODS_EXPORT void * ModsDrvAllocEvent(const char *Name) MODS_RETURN(0)

//! allocate event that has auto reset
MODS_EXPORT void * ModsDrvAllocEventAutoReset(const char *Name) MODS_RETURN(0)

//! allocate auto reset event which supports ModsDrvWaitOnMultipleEvents
MODS_EXPORT void * ModsDrvAllocSystemEvent(const char *Name) MODS_RETURN(0)

//! Self explanatory
MODS_EXPORT void   ModsDrvFreeEvent(void *hEvent) MODS_EMPTY_BODY

//! Self explanatory
MODS_EXPORT void   ModsDrvResetEvent(void *hEvent) MODS_EMPTY_BODY

//! Self explanatory
MODS_EXPORT void   ModsDrvSetEvent(void *hEvent) MODS_EMPTY_BODY

//! Self explanatory
MODS_EXPORT MODS_BOOL ModsDrvWaitOnEvent(void *hEvent, FLOAT64 TimeoutSecs) MODS_RETURN(MODS_FALSE)

MODS_EXPORT MODS_BOOL ModsDrvWaitOnMultipleEvents
(
    void**  hEvents,
    UINT32  numEvents,
    UINT32* pCompletedIndices,
    UINT32  maxCompleted,
    UINT32* pNumCompleted,
    FLOAT64 timeoutSec
) MODS_RETURN(MODS_FALSE)

//! Special event handling for the GPU resource manager
MODS_EXPORT void ModsDrvHandleResmanEvent(void *hOsEvent, UINT32 GpuInstance) MODS_EMPTY_BODY

//! Retrieve OS event usable with LwRmAllocEvent
MODS_EXPORT void *ModsDrvGetOsEvent(void *hOsEvent, UINT32 hClient, UINT32 hDevice) MODS_RETURN(0)

//! Create Semaphore (like Win32 semaphore)
MODS_EXPORT void* ModsDrvCreateSemaphore(UINT32 initCount, const char* name) MODS_RETURN(0)

//! Destroy Semaphore
MODS_EXPORT void ModsDrvDestroySemaphore(void *hSemaphore) MODS_EMPTY_BODY

//! Acquire Semaphore
MODS_EXPORT MODS_BOOL ModsDrvAcquireSemaphore(void *hSemaphore,
                                              FLOAT64 timeoutSec) MODS_RETURN(MODS_FALSE)

//! Release semaphore
MODS_EXPORT void ModsDrvReleaseSemaphore(void *hSemaphore) MODS_EMPTY_BODY

//! Acquire Spinlock
MODS_EXPORT void *ModsDrvAllocSpinLock(void) MODS_RETURN(0)

// Destroy Spinlock
MODS_EXPORT void ModsDrvDestroySpinLock(void *hSpinLock) MODS_EMPTY_BODY

// Acquire spinlock
MODS_EXPORT void ModsDrvAcquireSpinLock(void *hSpinLock) MODS_EMPTY_BODY

// Release spinlock
MODS_EXPORT void ModsDrvReleaseSpinLock(void *hSpinLock) MODS_EMPTY_BODY

//------------------------------------------------------------------------------
// CPU atomic functions
//------------------------------------------------------------------------------

//! Atomically read the value from memory.
MODS_EXPORT UINT32 ModsDrvAtomicRead(volatile UINT32* addr) MODS_RETURN(0)

//! Atomically store the value in memory.
MODS_EXPORT void ModsDrvAtomicWrite(volatile UINT32* addr, UINT32 data) MODS_EMPTY_BODY

//! Atomically swap the value at the memory location with the operand.
MODS_EXPORT UINT32 ModsDrvAtomicXchg32(volatile UINT32* addr, UINT32 data) MODS_RETURN(0)

//! Atomically put the new value at the memory location if the current value
//! at that memory location equals to the old value.
MODS_EXPORT MODS_BOOL ModsDrvAtomicCmpXchg32(volatile UINT32* addr, UINT32 oldVal, UINT32 newVal) MODS_RETURN(MODS_FALSE)

//! Atomically put the new value at the memory location if the current value
//! at that memory location equals to the old value.
MODS_EXPORT MODS_BOOL ModsDrvAtomicCmpXchg64(volatile UINT64* addr, UINT64 oldVal, UINT64 newVal) MODS_RETURN(MODS_FALSE)

//! Atomically add the operand to the value at the memory location.
MODS_EXPORT INT32 ModsDrvAtomicAdd(volatile INT32* addr, INT32 data) MODS_RETURN(0)

//------------------------------------------------------------------------------
// CPU caching functions
//------------------------------------------------------------------------------

#define LW_FLUSH_CPU_CACHE              1
#define LW_ILWALIDATE_CPU_CACHE         2
#define LW_FLUSH_ILWALIDATE_CPU_CACHE   (LW_FLUSH_CPU_CACHE | LW_ILWALIDATE_CPU_CACHE)
//! Flush the Cpu Cache (implementation dependent on CPU, MODS supports many)
MODS_EXPORT void   ModsDrvFlushCpuCache(void) MODS_EMPTY_BODY

//! Flush a Cpu Cache range (implementation dependent on CPU, MODS supports many)
MODS_EXPORT void   ModsDrvFlushCpuCacheRange(void * StartAddress,
                                             void * EndAddress,
                                             UINT32 Flags) MODS_EMPTY_BODY

//! Flush the Cpu's write combined buffer if applicable
MODS_EXPORT void   ModsDrvFlushCpuWriteCombineBuffer(void) MODS_EMPTY_BODY

//------------------------------------------------------------------------------
// PCI config space functions
//------------------------------------------------------------------------------

typedef enum
{
   PCI_OK,
   PCI_BIOS_NOT_PRESENT,
   PCI_FUNCTION_NOT_SUPPORTED,
   PCI_ILWALID_VENDOR_IDENTIFICATION,
   PCI_DEVICE_NOT_FOUND,
   PCI_ILWALID_REGISTER_NUMBER
} PciReturnCode;

MODS_EXPORT PciReturnCode ModsDrvFindPciDevice(UINT32 DeviceId,
                                               UINT32 VendorId, UINT32 Index,
                                               UINT32 *pDomain,
                                               UINT32 *pBus, UINT32 *pDevice,
                                               UINT32 *pFunction) MODS_RETURN(PCI_OK)
MODS_EXPORT PciReturnCode ModsDrvFindPciClassCode(UINT32 ClassCode,
                                                  UINT32 Index,
                                                  UINT32 *pDomain,
                                                  UINT32 *pBus,
                                                  UINT32 *pDevice,
                                                  UINT32 *pFunction) MODS_RETURN(PCI_OK)

// Function used by RM to scan PCI bus
MODS_EXPORT PciReturnCode ModsDrvPciInitHandle(UINT32 domain,
                                               UINT32 bus,
                                               UINT32 device,
                                               UINT32 function,
                                               UINT16* pVendorId,
                                               UINT16* pDeviceId) MODS_RETURN(PCI_OK)

MODS_EXPORT PciReturnCode ModsDrvPciGetBarInfo(UINT32 domain, UINT32 bus, UINT32 device,
                                               UINT32 function, UINT32 barIndex,
                                               PHYSADDR* pBaseAddress,
                                               UINT64* pBarSize) MODS_RETURN(PCI_OK)
MODS_EXPORT PciReturnCode ModsDrvPciGetIRQ(UINT32 domain, UINT32 bus,
                                           UINT32 device, UINT32 function,
                                           UINT32* pIrq) MODS_RETURN(PCI_OK)
MODS_EXPORT UINT08 ModsDrvPciRd08(UINT32 Domain,
                                  UINT32 Bus, UINT32 Device,
                                  UINT32 Function, UINT32 Address) MODS_RETURN(0)
MODS_EXPORT UINT16 ModsDrvPciRd16(UINT32 Domain,
                                  UINT32 Bus, UINT32 Device,
                                  UINT32 Function, UINT32 Address) MODS_RETURN(0)
MODS_EXPORT UINT32 ModsDrvPciRd32(UINT32 Domain,
                                  UINT32 Bus, UINT32 Device,
                                  UINT32 Function, UINT32 Address) MODS_RETURN(0)
MODS_EXPORT void   ModsDrvPciWr08(UINT32 Domain,
                                  UINT32 Bus, UINT32 Device,
                                  UINT32 Function, UINT32 Address,
                                  UINT08 Data) MODS_EMPTY_BODY
MODS_EXPORT void   ModsDrvPciWr16(UINT32 Domain,
                                  UINT32 Bus, UINT32 Device,
                                  UINT32 Function, UINT32 Address,
                                  UINT16 Data) MODS_EMPTY_BODY
MODS_EXPORT void   ModsDrvPciWr32(UINT32 Domain,
                                  UINT32 Bus, UINT32 Device,
                                  UINT32 Function, UINT32 Address,
                                  UINT32 Data) MODS_EMPTY_BODY

MODS_EXPORT MODS_RC ModsDrvPciEnableAtomics(UINT32      domain,
                                            UINT32      bus,
                                            UINT32      device,
                                            UINT32      func,
                                            MODS_BOOL * pbAtomicsEnabled,
                                            UINT32    * pAtomicTypes) MODS_RETURN(0)
MODS_EXPORT MODS_RC ModsDrvPciFlr(UINT32 domain,
                                  UINT32 bus,
                                  UINT32 device,
                                  UINT32 func) MODS_RETURN(0)

// Port I/O functions
MODS_EXPORT UINT08 ModsDrvIoRd08(UINT16 Address) MODS_RETURN(0)
MODS_EXPORT UINT16 ModsDrvIoRd16(UINT16 Address) MODS_RETURN(0)
MODS_EXPORT UINT32 ModsDrvIoRd32(UINT16 Address) MODS_RETURN(0)
MODS_EXPORT void   ModsDrvIoWr08(UINT16 Address, UINT08 Data) MODS_EMPTY_BODY
MODS_EXPORT void   ModsDrvIoWr16(UINT16 Address, UINT16 Data) MODS_EMPTY_BODY
MODS_EXPORT void   ModsDrvIoWr32(UINT16 Address, UINT32 Data) MODS_EMPTY_BODY

#define MODS_DEVTYPE_GPU 148

MODS_EXPORT MODS_BOOL ModsDrvGetSOCDeviceAperture
(
    UINT32 DevType,         // Device type, types defined in relocation table
    UINT32 Index,           // Index within the type (0 is the first device of that type)
    void** ppLinAddr,       // Returned CPU pointer to device aperture
    UINT32* pSize           // Returned size of device aperture
) MODS_RETURN(MODS_FALSE)

MODS_EXPORT MODS_BOOL ModsDrvGetSOCDeviceApertureByName
(
    const char* Name,       // Device name
    void** ppLinAddr,       // Returned CPU pointer to device aperture
    UINT32* pSize           // Returned size of device aperture
) MODS_RETURN(MODS_FALSE)

// Low-level memory allocator
#define ATTRIB_UC 1
#define ATTRIB_WC 2
#define ATTRIB_WT 3
#define ATTRIB_WP 4
#define ATTRIB_WB 5

// Note : these definitions must match those in core/include/memtypes.h
#define PROTECT_NONE       0x0
#define PROTECT_READABLE   0x1
#define PROTECT_WRITEABLE  0x2
#define PROTECT_EXELWTABLE 0x4
#define PROTECT_READ_WRITE (PROTECT_READABLE | PROTECT_WRITEABLE)
#define PROTECT_PROCESS_SHARED 0x8

MODS_EXPORT size_t ModsDrvGetPageSize(void) MODS_RETURN(0)
MODS_EXPORT void * ModsDrvAllocPagesForPciDev
(
    size_t    numBytes,
    MODS_BOOL contiguous,
    UINT32    addressBits,
    UINT32    pageAttrib,
    UINT32    domain,
    UINT32    bus,
    UINT32    device,
    UINT32    function
) MODS_RETURN(0)
MODS_EXPORT void * ModsDrvAllocPages(size_t NumBytes, MODS_BOOL Contiguous,
                                     UINT32 AddressBits, UINT32 PageAttrib,
                                     UINT32 Protect, UINT32 GpuInst) MODS_RETURN(0)
MODS_EXPORT void * ModsDrvAllocPagesAligned(size_t NumBytes, UINT32 PhysAlign,
                                            UINT32 AddressBits,
                                            UINT32 PageAttrib, UINT32 Protect,
                                            UINT32 GpuInst) MODS_RETURN(0)
MODS_EXPORT void   ModsDrvFreePages(void *Descriptor) MODS_EMPTY_BODY
MODS_EXPORT void * ModsDrvMapPages(void *Descriptor, size_t Offset,
                                   size_t Size, UINT32 Protect) MODS_RETURN(0)
MODS_EXPORT void   ModsDrvUnMapPages(void *VirtualAddress) MODS_EMPTY_BODY
MODS_EXPORT UINT64 ModsDrvGetPhysicalAddress(void *Descriptor, size_t Offset) MODS_RETURN(0)
MODS_EXPORT MODS_RC ModsDrvSetDmaMask(UINT32 domain,
                                      UINT32 bus,
                                      UINT32 device,
                                      UINT32 func,
                                      UINT32 numBits) MODS_RETURN(0)
MODS_EXPORT MODS_RC ModsDrvDmaMapMemory(UINT32 domain,
                                        UINT32 bus,
                                        UINT32 device,
                                        UINT32 func,
                                        void  *descriptor) MODS_RETURN(0)
MODS_EXPORT MODS_RC ModsDrvDmaUnmapMemory(UINT32 domain,
                                          UINT32 bus,
                                          UINT32 device,
                                          UINT32 func,
                                          void  *descriptor) MODS_RETURN(0)
MODS_EXPORT UINT64 ModsDrvGetMappedPhysicalAddress(UINT32 domain,
                                                   UINT32 bus,
                                                   UINT32 device,
                                                   UINT32 func,
                                                   void  *descriptor,
                                                   size_t offset) MODS_RETURN (0)
MODS_EXPORT void   ModsDrvReservePages(size_t NumBytes, UINT32 PageAttrib) MODS_EMPTY_BODY
MODS_EXPORT void * ModsDrvMapDeviceMemory(UINT64 PhysAddr, size_t NumBytes,
                                          UINT32 PageAttrib, UINT32 Protect) MODS_RETURN(0)
MODS_EXPORT void   ModsDrvUnMapDeviceMemory(void *VirtualAddr) MODS_EMPTY_BODY
MODS_EXPORT UINT32 ModsDrvSetMemRange(UINT64 PhysAddr, size_t NumBytes,
                                      UINT32 PageAttr) MODS_RETURN(0)
MODS_EXPORT void   ModsDrvUnSetMemRange(UINT64 PhysAddr, size_t NumBytes) MODS_EMPTY_BODY

// These are experimental functions for LwSwitch driver, bug 2666037.
// At the moment it is not clear whether we will ever need to implement
// the guts of these functions.  This functionality is known to be
// needed when SWIOTLB is active, which we do not support.  For dislwssion
// of SWIOTLB, see https://confluence.lwpu.com/display/GM/SWIOTLB
#define DIRECTION_BIDIRECTIONAL 0
#define DIRECTION_TO_DEVICE     1
#define DIRECTION_FROM_DEVICE   2
MODS_EXPORT MODS_BOOL ModsDrvSyncForDevice(UINT32 domain,
                                           UINT32 bus,
                                           UINT32 device,
                                           UINT32 func,
                                           void  *descriptor,
                                           UINT32 direction) MODS_RETURN(MODS_FALSE)
MODS_EXPORT MODS_BOOL ModsDrvSyncForCpu(UINT32 domain,
                                        UINT32 bus,
                                        UINT32 device,
                                        UINT32 func,
                                        void  *descriptor,
                                        UINT32 direction) MODS_RETURN(MODS_FALSE)

MODS_EXPORT MODS_BOOL ModsDrvGetCarveout(UINT64* pPhysAddr, UINT64* pSize) MODS_RETURN(MODS_FALSE)
MODS_EXPORT MODS_BOOL ModsDrvGetVPR(UINT64* pPhysAddr, UINT64* pSize) MODS_RETURN(MODS_FALSE)

MODS_EXPORT MODS_BOOL ModsDrvAllocPagesIlwPR(
    size_t numBytes,
    UINT64 *physAddr,
    void **sysMemDesc,
    UINT64 *sysMemOffset) MODS_RETURN(MODS_FALSE)

MODS_EXPORT MODS_BOOL ModsDrvGetGenCarveout(UINT64 *pPhysAddr, UINT64 *pSize, UINT32 id, UINT64 align) MODS_RETURN(MODS_FALSE)
MODS_EXPORT MODS_BOOL ModsDrvCallACPIGeneric(UINT32 GpuInst,
                                             UINT32 Function,
                                             void *Param1,
                                             void *Param2,
                                             void *Param3) MODS_RETURN(MODS_FALSE)
MODS_EXPORT MODS_BOOL ModsDrvLWIFMethod(UINT32 Function, UINT32 SubFunction,
                                        void *InParams, UINT16 InParamSize,
                                        UINT32 *OutStatus, void *OutData,
                                        UINT16 *OutDataSize) MODS_RETURN(MODS_FALSE)

// CPU functions
MODS_EXPORT MODS_BOOL ModsDrvCpuIsCPUIDSupported(void) MODS_RETURN(MODS_FALSE)
MODS_EXPORT void   ModsDrvCpuCPUID(UINT32 Op, UINT32 SubOp, UINT32 *pEAX,
                                   UINT32 *pEBX, UINT32 *pECX, UINT32 *pEDX) MODS_EMPTY_BODY

// Registry access functions.
MODS_EXPORT MODS_BOOL ModsDrvReadRegistryDword(const char * RegDevNode,
    const char * RegParmStr, UINT32 * pData) MODS_RETURN(MODS_FALSE)
MODS_EXPORT MODS_BOOL ModsDrvGpuReadRegistryDword(UINT32 GpuInstance,
                                                  const char * RegDevNode,
                                                  const char * RegParmStr,
                                                  UINT32 * pData) MODS_RETURN(MODS_FALSE)

MODS_EXPORT MODS_BOOL ModsDrvReadRegistryString(const char * RegDevNode,
    const char * RegParmStr, char * pData, UINT32 * pLength) MODS_RETURN(MODS_FALSE)
MODS_EXPORT MODS_BOOL ModsDrvGpuReadRegistryString(UINT32 GpuInstance,
                                                   const char * RegDevNode,
                                                   const char * RegParmStr,
                                                   char * pData,
                                                   UINT32 * pLength) MODS_RETURN(MODS_FALSE)

MODS_EXPORT MODS_BOOL ModsDrvReadRegistryBinary(const char * RegDevNode,
    const char * RegParmStr, UINT08 * pData, UINT32 * pLength) MODS_RETURN(MODS_FALSE)
MODS_EXPORT MODS_BOOL ModsDrvGpuReadRegistryBinary(UINT32 GpuInstance,
                                                   const char * RegDevNode,
                                                   const char * RegParmStr,
                                                   UINT08 * pData,
                                                   UINT32 * pLength) MODS_RETURN(MODS_FALSE)

MODS_EXPORT MODS_RC ModsDrvPackRegistry(void  * pRegTable,
                                        LwU32 * pSize) MODS_RETURN(MODS_FALSE)

// Time functions
MODS_EXPORT void   ModsDrvPause(void) MODS_EMPTY_BODY
MODS_EXPORT void   ModsDrvDelayUS(UINT32 Microseconds) MODS_EMPTY_BODY
MODS_EXPORT void   ModsDrvDelayNS(UINT32 Nanoseconds) MODS_EMPTY_BODY
MODS_EXPORT void   ModsDrvSleep(UINT32 Milliseconds) MODS_EMPTY_BODY
MODS_EXPORT UINT64 ModsDrvGetTimeNS(void) MODS_RETURN(0)
MODS_EXPORT UINT64 ModsDrvGetTimeUS(void) MODS_RETURN(0)
MODS_EXPORT UINT64 ModsDrvGetTimeMS(void) MODS_RETURN(0)
MODS_EXPORT void ModsDrvGetNTPTime(UINT32 *secs, UINT32 *msecs) MODS_EMPTY_BODY

// Interrupt functions
#define NORMAL_IRQL   0
#define ELEVATED_IRQL 1
MODS_EXPORT int       ModsDrvGetLwrrentIrql(void)     MODS_RETURN(0)
MODS_EXPORT int       ModsDrvRaiseIrql(int NewIrql)   MODS_RETURN(0)
MODS_EXPORT void      ModsDrvLowerIrql(int NewIrql)   MODS_EMPTY_BODY
MODS_EXPORT MODS_BOOL ModsDrvHookIRQ(UINT32 irq, ISR func,
                                     void* pCookie)   MODS_RETURN(MODS_FALSE)
MODS_EXPORT MODS_BOOL ModsDrvUnhookIRQ(UINT32 irq, ISR func,
                                       void* pCookie) MODS_RETURN(MODS_FALSE)
MODS_EXPORT MODS_BOOL ModsDrvHookInt(const IrqParams* pIrqInfo, ISR func,
                                     void* pCookie)   MODS_RETURN(MODS_FALSE)
MODS_EXPORT MODS_BOOL ModsDrvUnhookInt(const IrqParams* pIrqInfo, ISR func,
                                       void* pCookie) MODS_RETURN(MODS_FALSE)
// Read and write memory that may live inside a simulator
MODS_EXPORT UINT08 ModsDrvMemRd08(const volatile void *Address) MODS_RETURN(0)
MODS_EXPORT UINT16 ModsDrvMemRd16(const volatile void *Address) MODS_RETURN(0)
MODS_EXPORT UINT32 ModsDrvMemRd32(const volatile void *Address) MODS_RETURN(0)
MODS_EXPORT UINT64 ModsDrvMemRd64(const volatile void *Address) MODS_RETURN(0)
MODS_EXPORT void   ModsDrvMemWr08(volatile void *Address, UINT08 Data) MODS_EMPTY_BODY
MODS_EXPORT void   ModsDrvMemWr16(volatile void *Address, UINT16 Data) MODS_EMPTY_BODY
MODS_EXPORT void   ModsDrvMemWr32(volatile void *Address, UINT32 Data) MODS_EMPTY_BODY
MODS_EXPORT void   ModsDrvMemWr64(volatile void *Address, UINT64 Data) MODS_EMPTY_BODY
MODS_EXPORT void   ModsDrvMemCopy(volatile void *Dst,
                                  const volatile void *Src, size_t Count) MODS_EMPTY_BODY
MODS_EXPORT void   ModsDrvMemSet(volatile void *Dst,
                                 UINT08 Data, size_t Count) MODS_EMPTY_BODY

// Simulator functions
typedef enum
{
   SIM_MODE_HARDWARE,
   SIM_MODE_RTL,
   SIM_MODE_CMODEL,
   SIM_MODE_AMODEL,
} SimMode;

MODS_EXPORT MODS_BOOL ModsDrvSimChiplibLoaded(void) MODS_RETURN(MODS_FALSE)
MODS_EXPORT void   ModsDrvClockSimulator(void) MODS_EMPTY_BODY
MODS_EXPORT UINT64 ModsDrvGetSimulatorTime(void) MODS_RETURN(0)
MODS_EXPORT SimMode ModsDrvGetSimulationMode(void) MODS_RETURN(SIM_MODE_HARDWARE)
MODS_EXPORT MODS_BOOL ModsDrvExtMemAllocSupported(void) MODS_RETURN(MODS_FALSE)
MODS_EXPORT int    ModsDrvSimEscapeWrite(const char *Path, UINT32 Index,
                                         UINT32 Size, UINT32 Value) MODS_RETURN(0)
MODS_EXPORT int    ModsDrvSimEscapeRead(const char *Path, UINT32 Index,
                                        UINT32 Size, UINT32 *Value) MODS_RETURN(0)
MODS_EXPORT int    ModsDrvSimGpuEscapeWrite(UINT32 gpuId, const char *Path,
                                            UINT32 Index, UINT32 Size,
                                            UINT32 Value) MODS_RETURN(0)
MODS_EXPORT int    ModsDrvSimGpuEscapeRead(UINT32 gpuId, const char *Path,
                                           UINT32 Index, UINT32 Size,
                                           UINT32 *Value) MODS_RETURN(0)
MODS_EXPORT int    ModsDrvSimGpuEscapeWriteBuffer(UINT32 GpuId, const char *Path,
                                                  UINT32 Index, size_t Size,
                                                  const void *Buf) MODS_RETURN(0)
MODS_EXPORT int    ModsDrvSimGpuEscapeReadBuffer(UINT32 GpuId, const char *Path,
                                                 UINT32 Index, size_t Size,
                                                 void *Buf) MODS_RETURN(0)

// Amodel functions
MODS_EXPORT void   ModsDrvAmodAllocChannelDma(UINT32 ChID, UINT32 Class,
                                              UINT32 CtxDma,
                                              UINT32 ErrorNotifierCtxDma) MODS_EMPTY_BODY
MODS_EXPORT void   ModsDrvAmodAllocChannelGpFifo(UINT32 ChID, UINT32 Class,
                                                 UINT32 CtxDma,
                                                 UINT64 GpFifoOffset,
                                                 UINT32 GpFifoEntries,
                                                 UINT32 ErrorNotifierCtxDma) MODS_EMPTY_BODY
MODS_EXPORT void   ModsDrvAmodFreeChannel(UINT32 ChID) MODS_EMPTY_BODY
MODS_EXPORT void   ModsDrvAmodAllocContextDma(UINT32 ChID, UINT32 Handle,
                                              UINT32 Class, UINT32 Target,
                                              UINT32 Limit, UINT32 Base,
                                              UINT32 Protect) MODS_EMPTY_BODY
MODS_EXPORT void   ModsDrvAmodAllocObject(UINT32 ChID, UINT32 Handle,
                                          UINT32 Class) MODS_EMPTY_BODY
MODS_EXPORT void   ModsDrvAmodFreeObject(UINT32 ChID, UINT32 Handle) MODS_EMPTY_BODY

// GPU-specific functions
MODS_EXPORT UINT32 ModsDrvGpuRegWriteExcluded(UINT32 GpuInst, UINT32 Offset) MODS_RETURN(0)

MODS_EXPORT UINT32 ModsDrvRegRd32(const volatile void* address) MODS_RETURN(0)
MODS_EXPORT void   ModsDrvRegWr32(volatile void* address,
                                  UINT32 data) MODS_EMPTY_BODY
MODS_EXPORT void   ModsDrvLogRegRd(UINT32 GpuInst, UINT32 Offset,
                                   UINT32 Data, UINT32 Bits) MODS_EMPTY_BODY
MODS_EXPORT void   ModsDrvLogRegWr(UINT32 GpuInst, UINT32 Offset,
                                   UINT32 Data, UINT32 Bits) MODS_EMPTY_BODY

MODS_EXPORT MODS_RC   ModsDrvGpuPreVBIOSSetup(UINT32 GpuInstance) MODS_RETURN(0)
MODS_EXPORT MODS_RC   ModsDrvGpuPostVBIOSSetup(UINT32 GpuInstance) MODS_RETURN(0)
MODS_EXPORT void    * ModsDrvGetVbiosFromLws(UINT32 GpuInstance) MODS_RETURN(0)
MODS_EXPORT void      ModsDrvAddGpuSubdevice(const void* pPciDevInfo) MODS_EMPTY_BODY
MODS_EXPORT UINT32    ModsDrvGpuDisplaySelected(void) MODS_RETURN(0)
MODS_EXPORT MODS_BOOL ModsDrvGpuGrIntrHook(UINT32 GrIdx, UINT32 GrIntr, UINT32 ClassError,
                                           UINT32 Addr, UINT32 DataLo) MODS_RETURN(MODS_FALSE)
MODS_EXPORT MODS_BOOL ModsDrvGpuDispIntrHook(UINT32 Intr0, UINT32 Intr1) MODS_RETURN(MODS_FALSE)
MODS_EXPORT MODS_BOOL ModsDrvGpuRcCheckCallback(UINT32 GpuInstance) MODS_RETURN(MODS_FALSE)
MODS_EXPORT MODS_BOOL ModsDrvGpuRcCallback(UINT32 GpuInstance, UINT32 hClient,
                                           UINT32 hDevice, UINT32 hObject,
                                           UINT32 errorLevel, UINT32 errorType,
                                           void *pData,
                                           void *pRecoveryCallback) MODS_RETURN(MODS_FALSE)
MODS_EXPORT void ModsDrvGpuStopEngineScheduling(UINT32 GpuInstance,
                                                UINT32 EngineId,
                                                MODS_BOOL Stop) MODS_EMPTY_BODY
MODS_EXPORT MODS_BOOL ModsDrvGpuCheckEngineScheduling(UINT32 GpuInstance,
                                                      UINT32 EngineId) MODS_RETURN(MODS_FALSE)

// CheetAh functions to manipulate clocks
MODS_EXPORT MODS_BOOL ModsDrvClockGetHandle
(
    ModsClockDomain clockDomain,
    UINT32 clockIndex,
    UINT64 *pHandle
) MODS_RETURN(MODS_FALSE)
MODS_EXPORT MODS_BOOL ModsDrvClockEnable
(
    UINT64 clockHandle,
    MODS_BOOL enable
) MODS_RETURN(MODS_FALSE)
MODS_EXPORT MODS_BOOL ModsDrvClockSetRate
(
    UINT64 clockHandle,
    UINT64 rate
) MODS_RETURN(MODS_FALSE)
MODS_EXPORT MODS_BOOL ModsDrvClockGetRate
(
    UINT64 clockHandle,
    UINT64 *pRate
) MODS_RETURN(MODS_FALSE)
MODS_EXPORT MODS_BOOL ModsDrvClockSetParent
(
    UINT64 clockHandle,
    UINT64 parentClockHandle
) MODS_RETURN(MODS_FALSE)
MODS_EXPORT MODS_BOOL ModsDrvClockGetParent
(
    UINT64 clockHandle,
    UINT64 *pParentClockHandle
) MODS_RETURN(MODS_FALSE)
MODS_EXPORT MODS_BOOL ModsDrvClockReset
(
    UINT64 clockHandle,
    MODS_BOOL assert
) MODS_RETURN(MODS_FALSE)

// CheetAh functions to access GPIOs
MODS_EXPORT MODS_BOOL ModsDrvGpioEnable
(
    UINT32 bit,
    MODS_BOOL enable
) MODS_RETURN(MODS_FALSE)
MODS_EXPORT MODS_BOOL ModsDrvGpioRead
(
    UINT32 bit,
    UINT08* pValue
) MODS_RETURN(MODS_FALSE)
MODS_EXPORT MODS_BOOL ModsDrvGpioWrite
(
    UINT32 bit,
    UINT08 value
) MODS_RETURN(MODS_FALSE)
typedef enum
{
    MODS_GPIO_DIR_INPUT,
    MODS_GPIO_DIR_OUTPUT
} ModsGpioDir;
MODS_EXPORT MODS_BOOL ModsDrvGpioSetDirection
(
    UINT32 bit,
    ModsGpioDir dir
) MODS_RETURN(MODS_FALSE)
MODS_EXPORT MODS_BOOL ModsDrvI2CRead
(
    UINT32 port,
    UINT08 address,
    const void* pOutputBuffer,
    UINT32 outputSize,
    void* pInputBuffer,
    UINT32 inputSize
) MODS_RETURN(MODS_FALSE)
MODS_EXPORT MODS_BOOL ModsDrvI2CWrite
(
    UINT32 port,
    UINT08 address,
    const void* pOutputBuffer0,
    UINT32 outputSize0,
    const void* pOutputBuffer1,
    UINT32 outputSize1
) MODS_RETURN(MODS_FALSE)

// External functions to control the SMBUS GPIO Expander
MODS_EXPORT MODS_RC ModsDrvSmbGpioEnablePower(MODS_BOOL  bEnable) MODS_RETURN(0)
MODS_EXPORT MODS_RC ModsDrvSmbGpioEnableFbClamp(MODS_BOOL  bEnable) MODS_RETURN(0)
MODS_EXPORT MODS_RC ModsDrvSmbGpioEnablePexRst(MODS_BOOL  bEnable) MODS_RETURN(0)
MODS_EXPORT MODS_RC ModsDrvSmbGpioGetPwrGood(MODS_BOOL*  pPwrGood) MODS_RETURN(0)

// External functions to control GC6 power
MODS_EXPORT MODS_RC ModsDrvGC6PowerControl(UINT32 GpuInstance, UINT32 *pInOut) MODS_RETURN(0)

MODS_EXPORT MODS_BOOL ModsDrvHandleRmPolicyManagerEvent(UINT32 GpuInst, UINT32 EventId) MODS_RETURN(MODS_FALSE)

//! Sets GPU voltage on T124 and up.
MODS_EXPORT MODS_RC ModsDrvTegraSetGpuVolt
    (
        UINT32 requestedVoltageInMicroVolts,
        UINT32* pActualVoltageInMicroVolts
    ) MODS_RETURN(0)

//! Retrieves GPU voltage on T124 and up.
MODS_EXPORT MODS_RC ModsDrvTegraGetGpuVolt
    (
        UINT32 * pVoltageInMicroVolts
    ) MODS_RETURN(0)

typedef struct _MODS_TEGRA_GPU_VOLT_INFO
{
    UINT32 milwoltageuV;
    UINT32 maxVoltageuV;
    UINT32 stepVoltageuV;
} MODS_TEGRA_GPU_VOLT_INFO;

//! Retrieves information about GPU voltage on T124 and up.
MODS_EXPORT MODS_RC ModsDrvTegraGetGpuVoltInfo
    (
        MODS_TEGRA_GPU_VOLT_INFO * pVoltageInfo
    ) MODS_RETURN(0)

//! Retrieves chip info on T124 and up.
MODS_EXPORT MODS_RC ModsDrvTegraGetChipInfo
    (
        UINT32 * pGpuSpeedoHv,
        UINT32 * pGpuSpeedoLv,
        UINT32 * pGpuIddq,
        UINT32 * pChipSkuId
    ) MODS_RETURN(0)

//! Returns the lwrrently faked process ID, if any (@sa RMFakeProcess).
//! Always returns 0 if INCLUDE_RMTEST not defined.
MODS_EXPORT UINT32 ModsDrvGetFakeProcessID() MODS_RETURN(0)

MODS_EXPORT MODS_RC ModsDrvGetFbConsoleInfo
    (
        PHYSADDR * pBaseAddress,
        UINT64   * pSize
    ) MODS_RETURN(0)

typedef enum
{
    MODSDRV_PPC_TCE_BYPASS_DEFAULT
   ,MODSDRV_PPC_TCE_BYPASS_FORCE_ON
   ,MODSDRV_PPC_TCE_BYPASS_FORCE_OFF
} ModsDrvPpcTceBypassMode;

MODS_EXPORT MODS_RC ModsDrvSetupDmaBase
(
    UINT32                   domain,
    UINT32                   bus,
    UINT32                   device,
    UINT32                   func,
    ModsDrvPpcTceBypassMode  mode,
    UINT64                   devDmaMask,
    PHYSADDR *               pDmaBase
) MODS_RETURN(0)

//! Get the DMA start address (0 for everything but PPC)
MODS_EXPORT PHYSADDR ModsDrvGetGpuDmaBaseAddress(UINT32 gpuInstance) MODS_RETURN(0)
MODS_EXPORT MODS_RC ModsDrvGetPpcTceBypassMode
(
    UINT32 gpuInstance,
    ModsDrvPpcTceBypassMode *pBypassMode
) MODS_RETURN(0)

MODS_EXPORT MODS_RC ModsDrvGetAtsTargetAddressRange
(
    UINT32     gpuInst,
    UINT32    *pAddr,
    UINT32    *pAddrGuest,
    UINT32    *pAddrWidth,
    UINT32    *pMask,
    UINT32    *pMaskWidth,
    UINT32    *pGranularity,
    MODS_BOOL  bIsPeer,
    UINT32     peerIndex
) MODS_RETURN(0)

MODS_EXPORT MODS_RC ModsDrvGetLwLinkLineRate
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 func,
    UINT32 *pSpeed
) MODS_RETURN(0)

MODS_EXPORT MODS_RC ModsDrvGetForcedLwLinkConnections
(
    UINT32 gpuInst,
    UINT32 numLinks,
    UINT32 *pLinkArray
) MODS_RETURN(0)

MODS_EXPORT MODS_RC ModsDrvGetForcedC2CConnections
(
    UINT32 gpuInst,
    UINT32 numLinks,
    UINT32 *pLinkArray
) MODS_RETURN(0)

typedef enum
{
     MODSDRV_VAF_SHARED    = (0x1 << 0)
    ,MODSDRV_VAF_ANONYMOUS = (0x1 << 1)
    ,MODSDRV_VAF_FIXED     = (0x1 << 2)
    ,MODSDRV_VAF_PRIVATE   = (0x1 << 3)
} ModsDrvVirtualAllocFlags;
MODS_EXPORT MODS_RC ModsDrvVirtualAlloc(void **ppReturnedAddr, void *pBase, size_t size, UINT32 protect, UINT32 vaFlags) MODS_RETURN(0)

MODS_EXPORT void *ModsDrvVirtualFindFreeVaInRange(size_t size, void *pBase, void *pLimit, size_t align) MODS_RETURN(NULL)

typedef enum
{
    MODSDRV_VFT_DECOMMIT
   ,MODSDRV_VFT_RELEASE
} ModsDrvVirtualFreeType;
MODS_EXPORT MODS_RC ModsDrvVirtualFree(void *pAddr, size_t size, ModsDrvVirtualFreeType freeType) MODS_RETURN(0)
MODS_EXPORT MODS_RC ModsDrvVirtualProtect(void *pAddr, size_t size, UINT32 protect) MODS_RETURN(0)

typedef enum
{
    MODSDRV_MT_NORMAL
   ,MODSDRV_MT_FORK
   ,MODSDRV_MT_DONTFORK
} ModsDrvMadviseType;
MODS_EXPORT MODS_RC ModsDrvVirtualMadvise(void *pAddr, size_t size, ModsDrvMadviseType advice) MODS_RETURN(0)

MODS_EXPORT MODS_BOOL ModsDrvIsLwSwitchPresent(void) MODS_RETURN(MODS_FALSE)

MODS_EXPORT MODS_RC ModsDrvSetLwLinkSysmemTrained
(
    UINT32    domain,
    UINT32    bus,
    UINT32    device,
    UINT32    func,
    MODS_BOOL trained
) MODS_RETURN(0)

// Strings used as the "name" argument of the SharedSysmem functions
#define VGPU_GUEST_MEMORY_NAME "VgpuGmem"

//! Functions for allocating memory shared between multiple mods intances
MODS_EXPORT MODS_BOOL ModsDrvAllocSharedSysmem(
        const char *name, UINT64 id, size_t size, MODS_BOOL readOnly,
        void **ppAddr) MODS_RETURN(MODS_FALSE)
MODS_EXPORT MODS_BOOL ModsDrvImportSharedSysmem(
        UINT32 pid, const char *name, UINT64 id, MODS_BOOL readOnly,
        void **ppAddr, size_t *pSize) MODS_RETURN(MODS_FALSE)
MODS_EXPORT MODS_BOOL ModsDrvFreeSharedSysmem(
        void *pAddr) MODS_RETURN(MODS_FALSE)
MODS_EXPORT MODS_BOOL ModsDrvFindSharedSysmem(
        const char *cmp, UINT32 pid,
        char *name, size_t *pNameSize, UINT64 *pId) MODS_RETURN(MODS_FALSE)

MODS_EXPORT void*  ModsDrvTestApiGetContext() MODS_RETURN(nullptr)

MODS_EXPORT MODS_RC ModsDrvReadPFPciConfigIlwF(UINT32 addr, UINT32 *data) MODS_RETURN(0)

//! Read the data of a particular file out of a tar file.
//! Call once with pData=NULL to get the size and allocate enough memory to call it again with pData pointing at that memory
MODS_EXPORT MODS_RC ModsDrvReadTarFile(const char* tarfilename, const char* filename, UINT32* pSize, char* pData) MODS_RETURN(0)

MODS_EXPORT ModsOperatingSystem ModsDrvGetOperatingSystem() MODS_RETURN(MODS_OS_NONE)

#ifdef __cplusplus
}
#endif
