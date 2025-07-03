/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <falcon-intrinsics.h>
#include "fbflcn_defines.h"
#include "priv.h"


#ifndef SEQ_DECODE_H_
#define SEQ_DECODE_H_

extern LwBool gbl_en_fb_mclk_sw;
extern LwU32 gbl_pa_mclk_sw_seq[];
extern LwU32 gbl_pa_instr_cnt;
extern LwU16 gbl_prev_fbio_idx;

#define PA_MAX_SEQ_SIZE              0x300  // used to create seq array
#define SEQ_TIMER_GRAN_NS               37  // # ns per clock tick
// value is shifted since counter's 1st 4 bits are unused (for 1ns scaling)
#define POLL_TIMER_VAL         (0x3D09<<5)  // roughly 500us (clk=32ns)
//#define POLL_TIMER_VAL         (0xFFFFFF<<5)  // roughly 535ms (clk=32ns)
//#define POLL_TIMER_VAL        (0xFFFFFFF<<5)  // roughly 8s (clk=32ns)
#define SEQ_DONE                       0x1

#define PA_ERR_POLL_TIMER_EXPIRED       0x00100000
#define PA_ERR_TRNCHK_TIMER_EXPIRED     0x00110000
#define PA_ERR_NO_START                 0x00010000
#define PA_ERR_ILWALID_CMD              0x00020000
#define PA_ERR_BAD_CRC                  0x00050000
#define PA_ERR_SIZE_FAULT               0x00060000
#define PA_ERR_TRAINING                 0x000F0000
#define PA_FB_REQ_HALT                  0x00800000

#define PA_STATUS_DONE          (0x1<<0)
#define PA_STATUS_RUNNING       (0x1<<1)
#define PA_STATUS_RUN_MASK      0x3
#define PA_STATUS_SYNC_REQ      (0x1<<2)
#define PA_STATUS_SYNC_CLEAR    (0x3<<2)
#define PA_STATUS_SYNC_VALID    (LwU32)(0x1<<31)

#define PA_CTRL_START           0x1
#define PA_CTRL_SYNC_ACK        (0x1<<2)

//                                            # DWs (including OpCode)
#define SEQ_CMD_OPCODE_STORE    (0x00<<24)  // 2
#define SEQ_CMD_OPCODE_RMW      (0x01<<24)  // 3
#define SEQ_CMD_OPCODE_POLL     (0x02<<24)  // 3
#define SEQ_CMD_OPCODE_WR_STALL (0x03<<24)  // 2
// PA op code for lower subp if valid
#define SEQ_CMD_OPCODE_RMW_L    (0x04<<24)  // 3
#define SEQ_CMD_OPCODE_WAIT     (0x05<<24)  // 1
// reverse poll & halt
#define SEQ_CMD_OPCODE_TRNCHK   (0x06<<24)  // 1
#define SEQ_CMD_OPCODE_START    (0x07<<24)  // 1
#define SEQ_CMD_OPCODE_END      (0x08<<24)  // 1

typedef struct
{
    LwU16 poll;
    LwU16 ctrl;
    LwU8 fb_sync_cnt;
} FB_SYNC_STRUCT;

typedef struct
{
    LwU32 half_subp;
    LwU32 partitionNr;
} PAFLCN_SEQUENCER_INFO_FIELD;


LW_STATUS paStartSeq(void);

LwU32 pa_setup_timer(void);

LW_STATUS paDecode(LwU32* start, LwU32 size);

LwU32 gen_seq_crc32(const LwU32* loc, LwU32 size);


void seq_pa_write1(LwU32 val);
void seq_pa_write(LwU32 addr, LwU32 val);
void seq_pa_write_mask(LwU32 addr, LwU32 val, LwU32 mask);
LwU32 seqHalt(LwU32 pc, LwU32 data1, LwU32 data2, LwU32 err_code);
#define seq_pa_write_stall(_addr,_val)                      \
({                                                          \
    seq_pa_write((SEQ_CMD_OPCODE_WR_STALL|(_addr)),(_val)); \
})


#define GET_PC()        \
({                      \
	LwU32 pc;           \
	falc_rspr(pc,PC);   \
    pc;                 \
})

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))

//data &= mask?
#define SEQ_REG_WR32(_bus,_addr,_val)                       \
do {                                                        \
    if (!gbl_en_fb_mclk_sw) {                               \
        seq_pa_write((SEQ_CMD_OPCODE_STORE|(_addr)),(_val));\
    }                                                       \
    else {                                                  \
        _bus##_REG_WR32((_addr),(_val));                    \
    }                                                       \
} while (0)

#define SEQ_REG_WR32_STALL(_bus,_addr,_val)                 \
do {                                                        \
    if (!gbl_en_fb_mclk_sw) {                               \
        seq_pa_write((SEQ_CMD_OPCODE_WR_STALL|(_addr)),(_val));\
    }                                                       \
    else {                                                  \
        _bus##_REG_WR32_STALL((_addr),(_val));              \
    }                                                       \
} while (0)

#define SEQ_REG_RMW32(_bus,_addr,_val,_mask)                \
do {                                                        \
    if (!gbl_en_fb_mclk_sw) {                               \
        seq_pa_write_mask((SEQ_CMD_OPCODE_RMW|(_addr)),(_val),(_mask));   \
    } else {                                                \
        LwU32 data = _bus##_REG_RD32((_addr));              \
        data = (data & ~(_mask)) | ((_val) & (_mask));      \
        _bus##_REG_WR32((_addr),(data));                    \
    }                                                       \
} while (0)

// need to fix these for correct time
#define SEQ_WAIT_NS(_val)               \
do {                                    \
    if (!gbl_en_fb_mclk_sw) {           \
        seq_pa_write1((SEQ_CMD_OPCODE_WAIT |  \
            (0x00FFFFFF & (((_val)+SEQ_TIMER_GRAN_NS-1)/SEQ_TIMER_GRAN_NS))));   \
    } else {                            \
        OS_PTIMER_SPIN_WAIT_NS((_val)); \
    }                                   \
} while (0)

#define SEQ_WAIT_US(_val)               \
do {                                    \
    if (!gbl_en_fb_mclk_sw) {           \
        seq_pa_write1((SEQ_CMD_OPCODE_WAIT |  \
            (0x00FFFFFF & (((1000*(_val))+SEQ_TIMER_GRAN_NS-1)/SEQ_TIMER_GRAN_NS))));   \
    } else {                            \
        OS_PTIMER_SPIN_WAIT_US((_val)); \
    }                                   \
} while (0)

#define SEQ_REG_RMWL32(_bus,_addr,_val,_mask)               \
do {                                                        \
    seq_pa_write_mask((SEQ_CMD_OPCODE_RMW_L|(_addr)),(_val),(_mask));           \
} while (0)

#define SEQ_REG_POLL32(_addr,_val,_mask)                \
do {                                                    \
    seq_pa_write_mask((SEQ_CMD_OPCODE_POLL|(_addr)),(_val),(_mask));            \
} while (0)

// expected that the address is LW_PFB_FBPA_TRAINING_STATUS since it checks for
// LW_PFB_FBPA_TRAINING_STATUS_SUBP[x]_STATUS and LW_PFB_FBPA_TRAINING_STATUS_SUBP[x]_STATUS_RUNNING
#define SEQ_REG_TRNCHK(_addr)                           \
do {                                                    \
    seq_pa_write1((SEQ_CMD_OPCODE_TRNCHK | (_addr)));   \
} while (0)

#else

// fix this later !!
#define SEQ_REG_POLL32(_addr,_val,_mask)                \
do {                                                    \
} while (0)

#define SEQ_REG_WR32(_bus,_addr,_val)                       \
do {                                                        \
        _bus##_REG_WR32((_addr),(_val));                    \
} while (0)

#define SEQ_REG_WR32_STALL(_bus,_addr,_val)                 \
do {                                                        \
        _bus##_REG_WR32_STALL((_addr),(_val));              \
} while (0)

#define SEQ_REG_RMW32(_bus,_addr,_val,_mask)                \
do {                                                        \
        LwU32 data = _bus##_REG_RD32((_addr));              \
        data = (data & ~(_mask)) | ((_val) & (_mask));      \
        _bus##_REG_WR32((_addr),(data));                    \
} while (0)

// need to fix these for correct time
#define SEQ_WAIT_NS(_val)               \
do {                                    \
        OS_PTIMER_SPIN_WAIT_NS((_val)); \
} while (0)

#define SEQ_WAIT_US(_val)               \
do {                                    \
        OS_PTIMER_SPIN_WAIT_US((_val)); \
} while (0)

#endif



#define SEQ_PA_START(_size)  gbl_pa_mclk_sw_seq[0] = SEQ_CMD_OPCODE_START | _size
#define SEQ_PA_END()                    \
do {                                    \
    seq_pa_write1(SEQ_CMD_OPCODE_END);  \
} while (0)

#define FB_REG_WR32(_bus,_addr,_val)                    \
do {                                                    \
    if (gbl_en_fb_mclk_sw) {                            \
        _bus##_REG_WR32((_addr),(_val));                \
    }                                                   \
} while (0)

#define FB_REG_WR32_STALL(_bus,_addr,_val)              \
do {                                                    \
    if (gbl_en_fb_mclk_sw) {                            \
        _bus##_REG_WR32_STALL((_addr),(_val));          \
    }                                                   \
} while (0)


#endif /* SEQ_DECODE_H_ */
