#ifndef _UCODE_INTERFACE_H_
#define _UCODE_INTERFACE_H_
#include "lwtypes.h"

#ifdef UTF_BUILD_GH100_LWDEC_FMODEL
/* Version checking. If this file is updated,
 * it MUST be copied over to the MODS RMTEST
 * area for LWDEC testing */
#define __UCODE_IF_VERSION       1       /* Update this when making changes to this file */

#if __UCODE_IF_VERSION != __UCODE_IF_BUILD_VERSION
#error "ucode-interface.h version mismatch!! Copy ucode-interface.h to MODS RMTEST area, \
and update the makefile"
#endif
#endif // UTF_BUILD_GH100_LWDEC_FMODEL

// Limit number of tests to 2048 to reduce memory usage
#define MAX_NUM_TESTS         2048

// Line and page alignment are necessary to prevent nasty memory bugs
#define DMEM_LINE_LEN         0x10
#define DMEM_PAGE_LEN         0x100
#define UTF_ALIGN_UP(v, gran) (((v) + (gran-1)) & ~(gran-1))
#define UTF_LINE_ALIGN(v)     UTF_ALIGN_UP(v, DMEM_LINE_LEN)
#define UTF_PAGE_ALIGN(v)     UTF_ALIGN_UP(v, DMEM_PAGE_LEN)

// Tells UTF what to do when a test fails
typedef enum {
    ON_FAILURE_KEEP_GOING = 0,
    ON_FAILURE_STOP,
    ON_FAILURE_RESET
} FailurePlan;

typedef enum {
    ENGINE_ARCH_UNKNOWN = 0,
    ENGINE_ARCH_FALCON,
    ENGINE_ARCH_RISCV
} EngineArch;

typedef enum {
    ENGINE_NAME_UNKNOWN = 0,
    ENGINE_NAME_PMU,
    ENGINE_NAME_SEC2,
    ENGINE_NAME_GSP,
    ENGINE_NAME_LWDEC,
    ENGINE_NAME_FSP
} EngineName;

typedef struct {
    // magic number to insure that data has not been corrupted. Remove once we drop Falcon support.
    LwU32       magic;
    // Indicates that test exelwtion is complete
    LwBool      allDone;
    LwU16       numRan;
    LwU16       numPassed;
    LwU16       numFailed;
    LwBool      testPassed[MAX_NUM_TESTS];
} TestResults;

// Used by the Oracle/Loader to control UTF test exelwtion.
//
// numTestsToRun:
//     The number of tests to run from the testOrder array.
// idxTestToStart:
//     The index of the first test to run from the testOrder array.
// testOrder:
//     Contains the indices of the tests in the order in which they should
//     run. The test index is its order in the TESTS array in g_tests.c.
// failPlan:
//     What to do on test failure. The UTF can stop exelwtion (set allDone)
//     or reset the engine (set resetNeeded).
typedef struct {
    LwU16            numTestsToRun;
    LwU16            idxTestToStart;
    LwU16            testOrder[MAX_NUM_TESTS];
    FailurePlan      failPlan;
} TestControls;

// Set by the engine to make requests from the Loader.
//
// resetNeeded:
//     The engine is requesting a reset from the Loader.
typedef struct {
    LwBool          resetNeeded;
} TestRequests;

// Provides info to the ucode so it can determine what hardware it's running on.
// Useful for targetting multiple engines/architectures with a single binary.
//
// arch:
//     The architecture of the engine (Falcon/RISC-V).
// name:
//     The name of the engine (PMU, SEC2, GSP, etc.).
// base:
//     The address base of the engine (ex. 0x110000 on the RISC-V GSP)
typedef struct {
    EngineArch arch;
    EngineName name;
    LwU32      base;
} EngineInfo;

// Header for the DMESG ring buffer. Used by the ucode to print messages to the
// DMESG buffer, which are then extracted and sent to the Oracle for printing.
typedef struct {
    LwU32 read_offset;
    LwU32 write_offset;
    LwU32 buffer_size;
    LwU32 magic;
} DbgDmesgHdr;

#define UCODE_MAGIC    0xF007BA11U


// Below are the layouts of the UTF Block and DMESG buffer in memory. These
// May be in DMEM or FB (different on Falcon and Risc-v).
//
// These macros are intended for reference by the code. Changing these macros
// will affect where e.g. the Loader searches for the TestResults struct.
// However, to move something around, you will also need to modify the
// linker scripts (falcon/link.ld and riscv/link.ld).
//
// The UTF block refers to the set of TestResults, TestControls, TestRequests,
// and EngineInfo. DMESG refers to the 4 KiB DMESG buffer.

#define TEST_RESULTS_OFFSET   0
#define TEST_RESULTS_SIZE     sizeof(TestResults)
#define TEST_CONTROLS_OFFSET  UTF_LINE_ALIGN(TEST_RESULTS_OFFSET + TEST_RESULTS_SIZE)
#define TEST_CONTROLS_SIZE    sizeof(TestControls)
#define TEST_REQUESTS_OFFSET  UTF_LINE_ALIGN(TEST_CONTROLS_OFFSET + TEST_CONTROLS_SIZE)
#define TEST_REQUESTS_SIZE    sizeof(TestRequests)
#define ENGINE_INFO_OFFSET    UTF_LINE_ALIGN(TEST_REQUESTS_OFFSET + TEST_REQUESTS_SIZE)
#define ENGINE_INFO_SIZE      sizeof(EngineInfo)
#define UTF_BLOCK_SIZE        UTF_PAGE_ALIGN(ENGINE_INFO_OFFSET + ENGINE_INFO_SIZE)

#define UCODE_DMESG_SIZE      0x1000
#define UCODE_DMESG_HDR_SIZE  sizeof(DbgDmesgHdr)
#define UCODE_DMESG_BUF_SIZE  (UCODE_DMESG_SIZE - UCODE_DMESG_HDR_SIZE)

#endif // _UCODE_INTERFACE_H_
