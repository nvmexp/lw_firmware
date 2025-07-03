/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    vbios_opcodes.h
 * @brief   Contains macros which are the interfaces for PMGR mutexes.
 */

#ifndef PMU_VBIOS_OPCODES_H
#define PMU_VBIOS_OPCODES_H

/* ------------------------ System includes --------------------------------- */
/* ------------------------ Application includes ---------------------------- */
/* ------------------------ Public interfaces ------------------------------- */
FLCN_STATUS vbiosOpcode_NOT(VBIOS_IED_CONTEXT *pCtx, void *pData);
FLCN_STATUS vbiosOpcode_GENERIC_CONDITION(VBIOS_IED_CONTEXT *pCtx, void *pData);
FLCN_STATUS vbiosOpcode_CRTC(VBIOS_IED_CONTEXT *pCtx, void *pData);
FLCN_STATUS vbiosOpcode_POLL_LW(VBIOS_IED_CONTEXT *pCtx, void *pData);
FLCN_STATUS vbiosOpcode_SUB_DIRECT(VBIOS_IED_CONTEXT *pCtx, void *pData);
FLCN_STATUS vbiosOpcode_REG_ARRAY(VBIOS_IED_CONTEXT *pCtx, void *pData);
FLCN_STATUS vbiosOpcode_LW_COPY(VBIOS_IED_CONTEXT *pCtx, void *pData);
FLCN_STATUS vbiosOpcode_LW_REG(VBIOS_IED_CONTEXT *pCtx, void *pData);
FLCN_STATUS vbiosOpcode_RESETBITS_LW_REG(VBIOS_IED_CONTEXT *pCtx, void *pData);
FLCN_STATUS vbiosOpcode_SETBITS_LW_REG(VBIOS_IED_CONTEXT *pCtx, void *pData);
FLCN_STATUS vbiosOpcode_DPCD_REG(VBIOS_IED_CONTEXT *pCtx, void *pData);
FLCN_STATUS vbiosOpcode_DONE(VBIOS_IED_CONTEXT *pCtx, void *pData);
FLCN_STATUS vbiosOpcode_RESUME(VBIOS_IED_CONTEXT *pCtx, void *pData);
FLCN_STATUS vbiosOpcode_TIME(VBIOS_IED_CONTEXT *pCtx, void *pData);
FLCN_STATUS vbiosOpcode_CONDITION(VBIOS_IED_CONTEXT *pCtx, void *pData);
FLCN_STATUS vbiosOpcode_ZM_REG(VBIOS_IED_CONTEXT *pCtx, void *pData);
FLCN_STATUS vbiosOpcode_TIME_MSEC(VBIOS_IED_CONTEXT *pCtx, void *pData);

/* ------------------------ Types definitions ------------------------------- */

//
// The Devinit sequence itself is byte-packed. However, since the PMU cannot do 
// unaligned dword reads, we use a function called parseUnpackStructure() to store
// the Devinit opcodes in structs where every member is dword-aligned.
//
#define bios_U008  LwU32
#define bios_U016  LwU32
#define bios_U032  LwU32
#define bios_S008  LwS32
#define bios_S016  LwS32
#define bios_S032  LwS32

typedef struct
{
    LwU8  opcode;
    LwU8  hdrFmt;
} VBIOS_IED_OPCODE_MAP0;

typedef FLCN_STATUS(*FuncPtrDevinitOperation)
(
    VBIOS_IED_CONTEXT *pCtx,
    void  *pData
);

typedef struct
{
    FuncPtrDevinitOperation pFunc;
} VBIOS_IED_OPCODE_MAP1;

#define VBIOS_IED_CONDITION_ENTRY_SIZE  0x0000000c
#define VBIOS_IED_CONDITION_ENTRY_FMT   _3d_

typedef struct
{
    bios_U032   condAddress;
    bios_U032   condMask;
    bios_U032   condCompare;
} VBIOS_IED_CONDITION_ENTRY;

#define FMT_STRINGS(macro)                 \
    macro()                                \
    macro(1b)                              \
    macro(1b1w)                            \
    macro(2b)                              \
    macro(3b)                              \
    macro(4b1w)                            \
    macro(5b)                              \
    macro(5b1w)                            \
    macro(1w)                              \
    macro(1w2b)                            \
    macro(1d)                              \
    macro(1d1b)                            \
    macro(1d2b)                            \
    macro(1d1w)                            \
    macro(1d1s4d)                          \
    macro(1d3b2d1b)                        \
    macro(2d)                              \
    macro(3d)

#define FMT_STRUCT(fmt) char _##fmt##_[sizeof(#fmt)];
#define FMT_INIT(fmt)   #fmt,
typedef struct { FMT_STRINGS(FMT_STRUCT) } FmtStruct;

#define INIT_NOT                            0x38
#define INIT_NOT_HDR_FMT                    INIT_EMPTY_FMT
#define INIT_NOT_HDR                        INIT_EMPTY_HDR

#define INIT_GENERIC_CONDITION              0x3A
#define INIT_GENERIC_CONDITION_HDR_FMT      _2b_
typedef struct 
{
    bios_U008   condition;
    bios_U008   length;
} INIT_GENERIC_CONDITION_HDR;
#define CONDITION_ID_USE_EDP                0x00
#define CONDITION_ID_ASSR_SUPPORT           0x05
#define CONDITION_ID_NO_PANEL_SEQ_DELAYS    0x07

#define INIT_CRTC                           0x52
#define INIT_CRTC_HDR_FMT                   _3b_
typedef struct
{
    bios_U008   index;
    bios_U008   mask;
    bios_U008   data;
} INIT_CRTC_HDR;

#define INIT_POLL_LW                        0x56
#define INIT_POLL_LW_HDR_FMT                _2b_
typedef struct
{
    bios_U008   condition;
    bios_U008   timeout;        // 100 milliseconds
} INIT_POLL_LW_HDR;
#define POLL_INTERVAL_US    5

#define INIT_REG_ARRAY                      0x58
#define INIT_REG_ARRAY_HDR_FMT              _1d1b_
typedef struct
{
    bios_U032 addr;
    bios_U008 count;
} INIT_REG_ARRAY_HDR;

#define INIT_SUB_DIRECT                     0x5B
#define INIT_SUB_DIRECT_HDR_FMT             _1w_
typedef struct
{
    bios_U016   offset;
} INIT_SUB_DIRECT_HDR;

#define INIT_LW_COPY                        0x5F
#define INIT_LW_COPY_HDR_FMT                _1d1s4d_
typedef struct
{
    bios_U032   addr;
    bios_S008   shift;
    bios_U032   andMask;
    bios_U032   xorMask;
    bios_U032   destAddr;
    bios_U032   destAndMask;
} INIT_LW_COPY_HDR;

#define INIT_LW_REG                         0x6E
#define INIT_LW_REG_HDR_FMT                 _3d_
typedef struct
{
    bios_U032   addr;
    bios_U032   mask;
    bios_U032   data;
} INIT_LW_REG_HDR;

#define INIT_RESETBITS_LW_REG                0x47
#define INIT_RESETBITS_LW_REG_HDR_FMT       _2d_
typedef struct
{
    bios_U032   addr;
    bios_U032   data;
} INIT_RESETBITS_LW_REG_HDR;

#define INIT_SETBITS_LW_REG                 0x48

#define INIT_SETBITS_LW_REG_HDR_FMT         _2d_
typedef struct
{
    bios_U032   addr;
    bios_U032   data;
} INIT_SETBITS_LW_REG_HDR;

#define INIT_DPCD_REG                       0x98

#define INIT_DPCD_REG_HDR_FMT               _1d1b_
typedef struct
{
    bios_U032   startReg;
    bios_U008   count;
} INIT_DPCD_REG_HDR;

#define INIT_DONE                           0x71
#define INIT_DONE_HDR_FMT                   INIT_EMPTY_FMT
#define INIT_DONE_HDR                       INIT_EMPTY_HDR

#define INIT_RESUME                         0x72
#define INIT_RESUME_HDR_FMT                 INIT_EMPTY_FMT
#define INIT_RESUME_HDR                     INIT_EMPTY_HDR

#define INIT_TIME                           0x74
#define INIT_TIME_HDR_FMT                   _1w_
typedef struct
{
    bios_U016   delayUs;
} INIT_TIME_HDR;

#define INIT_CONDITION                      0x75
#define INIT_CONDITION_HDR_FMT              _1b_
typedef struct
{
    bios_U008   condition;
} INIT_CONDITION_HDR;

#define INIT_ZM_REG                         0x7A
#define INIT_ZM_REG_HDR_FMT                 _2d_
typedef struct
{
    bios_U032   addr;
    bios_U032   data;
} INIT_ZM_REG_HDR;

#define INIT_TIME_MSEC                      0x57
#define INIT_TIME_MSEC_HDR_FMT              _1w_
typedef struct
{
   bios_U016   delayMs;
} INIT_TIME_MSEC_HDR;


#define VBIOS_MAX_HEADER_SIZE_ALIGNED_BYTE 28 // From INIT_LW_COPY_HDR, adding one dword before and after.

#define VBIOS_MAX_BLOCK_SIZE_ALIGNED_BYTE  72 // From INIT_REG_ARRAY, adding one dword before and after.

#define VBIOS_MAX_BLOCK_SIZE_REG_ARRAY     (VBIOS_MAX_BLOCK_SIZE_ALIGNED_BYTE / sizeof(LwU32))

#define INIT_OPCODES(func)                 \
    func(NOT)                              \
    func(GENERIC_CONDITION)                \
    func(CRTC)                             \
    func(POLL_LW)                          \
    func(LW_REG)                           \
    func(DONE)                             \
    func(RESUME)                           \
    func(CONDITION)                        \
    func(ZM_REG)                           \
    func(TIME)                             \
    func(REG_ARRAY)                        \
    func(LW_COPY)                          \
    func(SUB_DIRECT)                       \
    func(RESETBITS_LW_REG)                 \
    func(DPCD_REG)                         \
    func(SETBITS_LW_REG)                   \
    func(TIME_MSEC)

#define OPCODE_HDR(op)    INIT_##op##_HDR        hdr_##op;
#define OPCODE_MAP0(op) { INIT_##op, offsetof(FmtStruct,INIT_##op##_HDR_FMT) },
#define OPCODE_MAP1(op) { vbiosOpcode_##op },

#define           INIT_EMPTY_FMT __
typedef struct {} INIT_EMPTY_HDR;

typedef union
{
    INIT_OPCODES(OPCODE_HDR)
} INIT_OPCODE_HDR;

/* ------------------------ External definitions ---------------------------- */
/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Macros ------------------------------------------ */

#endif  // PMU_VBIOS_OPCODES_H
