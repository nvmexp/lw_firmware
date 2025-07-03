/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef FBFLCN_HELPERS_H
#define FBFLCN_HELPERS_H

#include "fbflcn_defines.h"
#include "lwuproc.h"

// event definition:
//  debug printout is limitied to the lowest 24 bit due to a prefix header in the msb.
#define EVENT_MASK_CLEAR (0x0)
#define EVENT_MASK_CMD_QUEUE0 (1<<0)
#define EVENT_MASK_CMD_QUEUE1 (1<<1)
#define EVENT_MASK_CMD_QUEUE2 (1<<2)
#define EVENT_MASK_CMD_QUEUE3 (1<<3)
#define EVENT_MASK_TIMER_EVENT (1<<8)
#define EVENT_MASK_IEEE1500_TEMP_MEASURE (1<<9)
#define EVENT_MASK_TEMP_TRACKING (1<<10)
#define EVENT_MASK_IEEE1500_PERIODIC_TR (1<<11)
#define EVENT_MASK_EXELWTE_MCLK_SWITCH (1<<16)
#define EVENT_SAVE_TRAINING_DATA (1<<17)


#define max(a,b) \
		({ __typeof__ (a) _a = (a); \
		__typeof__ (b) _b = (b); \
		_a > _b ? _a : _b; })

#define min(a,b) \
		({ __typeof__ (a) _a = (a); \
		__typeof__ (b) _b = (b); \
		_a < _b ? _a : _b; })

/*
 * falcon halt function macro
 */
#define FBFLCN_HALT(code) \
{                         \
	LwU32 pc;             \
	falc_rspr(pc,PC);     \
	falc_stxb_i((LwU32*)(LW_CFBFALCON_FIRMWARE_MAILBOX(14)), 0, pc); \
	falc_stxb_i((LwU32*)(LW_CFBFALCON_FIRMWARE_MAILBOX(15)), 0, code); \
	falc_halt();  \
}

/*
 * special for nitroC
 */
#define NIO_PRINT(...) { }
#define NIO_ERROR(fmt,...) { }

#define LIMIT(a,b,code) \
        ({ __typeof__ (a) _a = (a); \
        __typeof__ (b) _b = (b); \
        if (_a > _b) { FBFLCN_HALT(code); }})
/*
 * Event Handling
 */
extern volatile LwU32 gEventVector;
void enableEvent(LwU32 eventMask);
void disableEvent(LwU32 eventMask);
LwBool testEvent(LwU32 eventMask);
void clearEvent(void);

#include "config/fbfalcon-config.h"
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
void enableIntEvent(LwU32 eventMask);
void joinIntEvent(void);
LwBool testIntEvent(LwU32 eventMask);
#endif

/*
 * lock and unlock for atomic code segements
 */
void lock(void);
void unlock(void);

/*
 * memcopy operations
 */
void* memcpyLwU8(void *s1, const void *s2, LwU32 n);
void *memsetLwU8(void *s1, LwU8 val, LwU32 n);

/*
 * parition handling
 */

extern LwU32 gDisabledFBPAMask;
extern LwU8  gNumberOfPartitions;
extern LwU32 gLowestIndexValidPartition;
extern LwU32 localEnabledFBPAHalfPartitionMask;
extern LwU32 localDisabledFbioMask;
extern LwU32 localEnabledFBPAMask;

// getter function for the partition enabled bit
LwBool isPartitionEnabled(LwU8 inx);
// getter function for half partition enabled bit
LwBool isLowerHalfPartitionEnabled(LwU8 inx);
static inline LwBool isUpperHalfPartitionEnabled(LwU8 inx) GCC_ATTRIB_ALWAYSINLINE();
LwU32 unicast (LwU32 priv_addr, LwU32 partition);
LwU32 detectPLLFrequencyMHz(void);
void delay_us (LwU32 delayInMicroSec);
void detectLowestIndexValidPartition(void);
static inline LwU32 getPartitionMask(void) GCC_ATTRIB_ALWAYSINLINE();
static inline LwU32 getDisabledPartitionMask(void) GCC_ATTRIB_ALWAYSINLINE();
static inline LwU32 unicastMasterReadFromBroadcastRegister (LwU32 priv_addr) GCC_ATTRIB_ALWAYSINLINE();
static inline LwU32 multicastMasterWriteFromBrodcastRegister (LwU32 priv_addr) GCC_ATTRIB_ALWAYSINLINE();

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GH10X))
//Helper functions for L2 slice disable
void read_l2_slice_disable_fuse(void);
LwBool isL2SliceDisabled(LwU8 pa_idx, LwU8 subp_idx, LwU8 channel_idx);
LwBool isSubpDisabled(LwU8 pa_idx, LwU8 subp_idx);
#endif
//
// inline functions
//
static inline LwBool isUpperHalfPartitionEnabled(LwU8 inx) { return isPartitionEnabled(inx);}
static inline LwU32 getPartitionMask(void) { return localEnabledFBPAMask;}
static inline LwU32 getDisabledPartitionMask(void) { return localDisabledFbioMask;}

// Translate broadcast read to single unicast read for serial priv
// Should be used to translate address for every ldx load from serial priv
static inline LwU32
unicastMasterReadFromBroadcastRegister
(
		LwU32 priv_addr
)
{
	LwU32 retval = priv_addr + gLowestIndexValidPartition - 0x000a0000;
	return retval;
}

// Translate generic priv access to the controlling MC group accessing serial priv
// at the moment this is programmed in vbios into MC2 (multicast 2)
// mcmaster_write_from_bcreg
static inline LwU32
multicastMasterWriteFromBrodcastRegister
(
		LwU32 priv_addr
)
{
	LwU32 retval = priv_addr + 0x00088000 - 0x000a0000;
	return retval;
}



#endif
