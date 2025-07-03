/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @brief   Structure and macro definitions for Performance Table 6.X
 */

#ifndef PSTATES_VBIOS_6X
#define PSTATES_VBIOS_6X

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp_src_vbios.h"

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * Maximum size of a PSTATE 6.X table that can be supported
 */
#define PSTATES_VBIOS_6X_MAX_SIZE \
    (sizeof(PSTATES_VBIOS_6X_HEADER) + \
     (LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS * \
        (sizeof(PSTATES_VBIOS_6X_ENTRY) + \
        (PSTATES_VBIOS_6X_CLK_DOMAINS_MAX * sizeof(PSTATES_VBIOS_6X_ENTRY_CLOCK)))))

/* ------------------------ VBIOS Definitions ------------------------------- */
//
// P-state Table Header 6.x
//
// The P-state Table Header is pointed at by the "Performance Table Pointer"
// field in the BIOS Information Tokens (BIT) BIT_PERF_PTRS structure. The
// P-state Table starts with a Header followed immediately by an array of
// Entries, consisting of a Base Entry and an array of Sub-Entries.
//
// https://wiki.lwpu.com/engwiki/index.php/AVFS/VBIOS_Changes_and_Proposals#Pstate_Table_6.0
//
#define PSTATES_VBIOS_6X_TABLE_VERSION                         0x60

#define PSTATES_VBIOS_6X_HEADER_SIZE_02                        0x02
#define PSTATES_VBIOS_6X_HEADER_SIZE_0A                        0x0A

typedef struct GCC_ATTRIB_PACKED()
{
    LwU8 version;                  // Version
    LwU8 headerSize;               // Size of header
    LwU8 baseEntrySize;            // Size of each base entry in bytes (not including clock info info)
    LwU8 baseEntryCount;           // Number of PerfLevel entries
    LwU8 clockEntrySize;           // Size of per clock entry
    LwU8 clockEntryCount;          // Number of clock Entries
    LwU8 flags0;                   // Flags
    LwU8 initialPState;            // VBIOS devinit will match this PState
    LwU8 cpiSupportLevel;          // CPI support level (not used by RM)
    LwU8 cpiFeatures;              // CPI feature bits (not used by RM)
} PSTATES_VBIOS_6X_HEADER;

#define PSTATES_VBIOS_6X_HEADER_FLAGS0_PSTATES                               0:0
#define PSTATES_VBIOS_6X_HEADER_FLAGS0_PSTATES_NOT_REQUIRED                    0
#define PSTATES_VBIOS_6X_HEADER_FLAGS0_PSTATES_REQUIRED                        1
#define PSTATES_VBIOS_6X_HEADER_FLAGS0_ARBITER_LOCK                          1:1
#define PSTATES_VBIOS_6X_HEADER_FLAGS0_ARBITER_LOCK_DISABLED                   0
#define PSTATES_VBIOS_6X_HEADER_FLAGS0_ARBITER_LOCK_ENABLED                    1
#define PSTATES_VBIOS_6X_HEADER_FLAGS0_LWDA_LIMIT_OVERRIDE_SUPPORT           2:2
#define PSTATES_VBIOS_6X_HEADER_FLAGS0_LWDA_LIMIT_OVERRIDE_SUPPORT_DISABLED    0
#define PSTATES_VBIOS_6X_HEADER_FLAGS0_LWDA_LIMIT_OVERRIDE_SUPPORT_ENABLED     1
#define PSTATES_VBIOS_6X_HEADER_FLAGS0_RSVD                                  7:3

//
// P-state Table Base Entry 6.x
//
// Each entry is made up of a single Base Entry and multiple clock entries.
// The entire size of an entry is given by (PstateTableHeader.BaseEntrySize +
// PstateTableHeader.ClockEntrySize * PstateTableHeader.ClockEntryCount).
//
// https://wiki.lwpu.com/engwiki/index.php/AVFS/VBIOS_Changes_and_Proposals#Pstate_Base_Entry
//
#define PSTATES_VBIOS_6X_BASE_ENTRY_SIZE_2                     0x2
#define PSTATES_VBIOS_6X_BASE_ENTRY_SIZE_3                     0x3
#define PSTATES_VBIOS_6X_BASE_ENTRY_SIZE_5                     0x5

/*!
 * Maximum number of supported CLK_DOMAINs
 */
#define PSTATES_VBIOS_6X_CLK_DOMAINS_MAX \
    LW2080_CTRL_BOARDOBJ_MAX_BOARD_OBJECTS

#define PSTATES_VBIOS_6X_ENTRY_CLOCK_SIZE_06                   0x6

typedef struct GCC_ATTRIB_PACKED()
{
    LwU16 param0;                   // Usage-Specific Parameter Word0
    LwU32 param1;                   // Usage-Specific Parameter Dword1
} PSTATES_VBIOS_6X_ENTRY_CLOCK;

typedef struct GCC_ATTRIB_PACKED()
{
    LwU8 pstateLevel;              // PState level number
    LwU8 flags0;                   // Flags
    LwU8 lpwrEntryIdx;             // Index to LowPower Entry Idx Table
    LwU8 pcieIdx;                  // Index to PCIe Table
    LwU8 lwlinkIdx;                // Index to LWLINK Table
} PSTATES_VBIOS_6X_ENTRY;

/*!
 * @defgroup    PSTATES_VBIOS_6X_PERFLEVEL
 *
 * VBIOS Pstate number defines
 *
 * @{
 */
#define PSTATES_VBIOS_6X_PERFLEVEL_SKIP_ENTRY   0xFF
#define PSTATES_VBIOS_6X_PERFLEVEL_PSTATE_P0    0x0F
#define PSTATES_VBIOS_6X_PERFLEVEL_PSTATE_P1    0x0E
#define PSTATES_VBIOS_6X_PERFLEVEL_PSTATE_P2    0x0D
#define PSTATES_VBIOS_6X_PERFLEVEL_PSTATE_P3    0x0C
#define PSTATES_VBIOS_6X_PERFLEVEL_PSTATE_P4    0x0B
#define PSTATES_VBIOS_6X_PERFLEVEL_PSTATE_P5    0x0A
#define PSTATES_VBIOS_6X_PERFLEVEL_PSTATE_P6    0x09
#define PSTATES_VBIOS_6X_PERFLEVEL_PSTATE_P7    0x08
#define PSTATES_VBIOS_6X_PERFLEVEL_PSTATE_P8    0x07
#define PSTATES_VBIOS_6X_PERFLEVEL_PSTATE_P9    0x06
#define PSTATES_VBIOS_6X_PERFLEVEL_PSTATE_P10   0x05
#define PSTATES_VBIOS_6X_PERFLEVEL_PSTATE_P11   0x04
#define PSTATES_VBIOS_6X_PERFLEVEL_PSTATE_P12   0x03
#define PSTATES_VBIOS_6X_PERFLEVEL_PSTATE_P13   0x02
#define PSTATES_VBIOS_6X_PERFLEVEL_PSTATE_P14   0x01
#define PSTATES_VBIOS_6X_PERFLEVEL_PSTATE_P15   0x00
/*!@}*/

#define PSTATES_VBIOS_6X_FLAGS0_PSTATE_CAP                       1:0
#define PSTATES_VBIOS_6X_FLAGS0_PSTATE_CAP_RM_DEFAULT              0   // P-state capping behavior is determined by the RM defaults
#define PSTATES_VBIOS_6X_FLAGS0_PSTATE_CAP_GLITCHY                 1   // P-state capping may cause screen glitch  (not waiting for VBLANK)
#define PSTATES_VBIOS_6X_FLAGS0_PSTATE_CAP_WAIT_VBLANK             2   // P-state capping will not cause screen glitch (synced with VBLANK)

#define PSTATES_VBIOS_6X_FLAGS0_LWDA                             2:2   // Is P-state too fast for LWCA? (Bug 1499038)
#define PSTATES_VBIOS_6X_FLAGS0_LWDA_SAFE                          0   // Safe to use this P-state when LWCA is running.
#define PSTATES_VBIOS_6X_FLAGS0_LWDA_NOT_SAFE                      1   // Do not use this P-state when LWCA is running.

#define PSTATES_VBIOS_6X_FLAGS0_OVOC                             3:3   // Enables/disables OV and OC interfaces for a given pstate.
#define PSTATES_VBIOS_6X_FLAGS0_OVOC_DISABLED                      0   // Disabled.
#define PSTATES_VBIOS_6X_FLAGS0_OVOC_ENABLED                       1   // Enabled.

#define PSTATES_VBIOS_6X_FLAGS0_DECREASE_THRESHOLD               4:4   // Ignore For Decrease Threshold Callwlation: FB Utilization (bug 1788174)
#define PSTATES_VBIOS_6X_FLAGS0_DECREASE_THRESHOLD_DEFAULT         0   // Callwalte FB utilization decrease thresholds normally using this P-State.
#define PSTATES_VBIOS_6X_FLAGS0_DECREASE_THRESHOLD_IGNORE_FB       1   // Ignore this P-State when callwlating FB utilization decrease thresholds.

// P-state Clock Entry param fields for usage != fixed
#define PSTATES_VBIOS_6X_CLOCK_PROG_PARAM0_NOM_FREQ_MHZ         13:0   // Nominal frequency (MHz).

#define PSTATES_VBIOS_6X_CLOCK_PROG_PARAM1_MIN_FREQ_MHZ         13:0   // Minimum frequency (MHz).
#define PSTATES_VBIOS_6X_CLOCK_PROG_PARAM1_MAX_FREQ_MHZ        27:14   // Maximum frequency (MHz).

/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Compile-Time Asserts ---------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc pstatesVbiosBoardObjGrpSrcInit
 *
 * @note    Version specific to @ref PSTATE_VBIOS_TABLE_VERSION_6X
 */
FLCN_STATUS pstatesVbiosBoardObjGrpSrcInit_6X(BOARDOBJGRP_SRC_VBIOS *pSrc)
    GCC_ATTRIB_SECTION("imem_libPstateBoardObjInit", "pstatesVbiosBoardObjGrpSrcInit_6X");

#endif // PSTATES_VBIOS_6X
