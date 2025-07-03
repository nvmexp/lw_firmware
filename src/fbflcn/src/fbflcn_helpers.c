/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <falcon-intrinsics.h>
//#include <falc_debug.h>
//#include <falc_trace.h>

#include "config/fbfalcon-config.h"

// include manuals
#include "dev_pri_ringmaster.h"
#include "dev_fbpa.h"
#include "dev_fuse.h"
#include "dev_fbfalcon_csb.h"

#include "priv.h"
#include "fbflcn_defines.h"
#include "fbflcn_helpers.h"

// someo of the bios headers seem to use Lw64 instead of LwU64
#define Lw64 LwU64

#if (!FBFALCONCFG_FEATURE_ENABLED(SUPPORT_NITROC))
#include "vbios/ucode_interface.h"
#include "vbios/ucode_postcodes.h"
#include "vbios/cert20.h"
#include "memory.h"

// sw/main/bios/core88/chip/gv100/pmu/inc/dev_gc6_island_addendum.h
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"

#include "config/fbfalcon-config.h"

#include "fbflcn_objfbflcn.h"
#include "config/g_fbfalcon_private.h"
#endif
extern OBJFBFLCN Fbflcn;
/*
 *-----------------------------------------------------------------------------
 * Event vector helper functions
 * The event vector keeps track of operation to perform in the main loop.
 * These helper functions allow to set / clear and test for an event.
 *
 * Notice: The event can be set and cleared inside interrupts, as they result
 * in a read-modify-write of the event vector we need to guarantee that this
 * is an atomic operation. At the moment we do this by temporarily disable
 * the interrupt
 *-----------------------------------------------------------------------------
 */

volatile LwU32 gEventVector = EVENT_MASK_CLEAR;

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GH10X))
#include "memory.h"
LwBool l2_slice_disabled[(MAX_PARTS*CHANNELS_IN_PA)]; //One bit per slice
#endif

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
static volatile LwU32 gIntEventVector = EVENT_MASK_CLEAR;
static volatile LwBool gIsLocked = LW_FALSE;

void
enableIntEvent
(
        LwU32 eventMask
)
{
    gIntEventVector = gIntEventVector | (eventMask);
}

void
joinIntEvent
(
        void
)
{
    if (!gIsLocked)
    {
        lock();
        gEventVector = gEventVector | gIntEventVector;
        gIntEventVector = EVENT_MASK_CLEAR;
        unlock();
    }
    else
    {
        gEventVector = gEventVector | gIntEventVector;
        gIntEventVector = EVENT_MASK_CLEAR;
    }
}

LwBool
testIntEvent
(
        LwU32 eventMask
)
{
    return ((gIntEventVector & eventMask) > 0);
}
#endif

void
enableEvent
(
        LwU32 eventMask
)
{
    gEventVector = gEventVector | (eventMask);
}

void
disableEvent
(
        LwU32 eventMask
)
{
    gEventVector = gEventVector & (~eventMask);
}

void
clearEvent
(
        void
)
{
    gEventVector = EVENT_MASK_CLEAR;
}

LwBool
testEvent
(
        LwU32 eventMask
)
{
    return ((gEventVector & eventMask) > 0);
}

/*
 *-----------------------------------------------------------------------------
 * lock/unlock routines to achieve atomic code block exelwtion
 * Purpose of this code is to protect access to variables that are use in
 * interrupt logic and that could create a race hazard between a main thread
 * access and an interrupt access. Without multithreading in the ucode itself
 * there is no need for a mutex, simply disabling the interrupt will
 * guarantee atomic exelwtion of the code encapsulated inside lock() and
 * unlock()
 *-----------------------------------------------------------------------------
 */

volatile LwU32 gPreCSW;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
static volatile LwBool gIsLockSaved = LW_FALSE;

void
lock()
{
    if (!gIsLockSaved)
    {
        falc_rspr(gPreCSW,CSW);
        gIsLockSaved = LW_TRUE;
    }

    if (!gIsLocked)
    {
        falc_wspr(CSW,0);
        gIsLocked = LW_TRUE;
    }
}

void
unlock()
{
    falc_wspr(CSW,gPreCSW);
    gIsLocked = LW_FALSE;
    gIsLockSaved = LW_FALSE;
}

#else

void
lock()
{
        falc_rspr(gPreCSW,CSW);
        falc_wspr(CSW,0);
}

void
unlock()
{
    falc_wspr(CSW,gPreCSW);
}
#endif


/*!
 * Simple memcopy, moving n char's from s2 to s1
 * @param[in]  s1   void pointer to the start of the destination buffer
 * @param[in]  s2   pointer to the start of the source buffer
 * @param[in]  n    number of bytes to be copied.
 */
void*
memcpy
(
        void *s1,
        const void *s2,
        LwU32 n
)
{
    LwU32 index;
    for (index = 0; index < n; index++){
        ((LwU8*)s1)[index] = ((LwU8*)s2)[index];
    }
    return s1;
}

/*!
 * memset, filling memory with n char's starting at s1
 * @param[in]  s1   void pointer to the start of the buffer
 * @param[in]  val  value to be fill buffer with
 * @param[in]  n    number of bytes to be cleared.
 */
void*
memsetLwU8
(
        void *s1,
        LwU8 val,
        LwU32 n
)
{
    LwU32 index;
    for (index = 0; index < n; index++){
        ((LwU8*)s1)[index] = (LwU8)val;
    }
    return s1;
}

//-----------------------------------------------------------------------------

LW_STATUS initStatus;

// partition masks are kept locally here, to access pls use the getter function calls
// these should be accesses in the rest of the ucode only through getter functions
// isPartitionEnabled and isHalfPartitionEnabled.
LwU32 localEnabledFBPAMask;
LwU32 localEnabledFBPAHalfPartitionMask;
LwU32 localDisabledFbioMask;

// these are for now ok w/ global access
LwU32 gDisabledFBPAMask;         // should only be used for gv100 code
LwU8  gNumberOfPartitions;
LwU32 gLowestIndexValidPartition;

// getter function for the partition enable mask, inx [0,X]
LwBool
isPartitionEnabled
(
        LwU8 inx
)
{
#if (!FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_MCLK_HBM_GH100))
    // Max 16 partitions are lwrrently supported
    if (inx > 15)
    {
        return LW_FALSE;
    }
#else
    //[bswamy] Updated for GH100
    // Max 24 partitions are lwrrently supported
    if (inx > (MAX_PARTS-1))
    {
        return LW_FALSE;
    }
#endif  //GH100
    // look up partition bit in the enable mask
    LwU32 partitionBit = ((localEnabledFBPAMask & (0x1 << inx) ) >> inx) & 0x1;
    return (partitionBit != 0);
}



LwBool isLowerHalfPartitionEnabled
(
        LwU8 inx
)
{
    return (isPartitionEnabled(inx) && (fbfalconIsHalfPartitionEnabled_HAL(&Fbflcn,inx) == LW_FALSE));
}


// detectLowestIndexValidPartition
//  this detects the lowest index partition that is working in the system and
//  saves its offset into gbl_fbpa_ucread_offs offset will be used in
//  ucMasterAccessFromBroadcastReg to replace broadcast read for priv on the
//  serial priv interface exelwted once at initialization


void
detectLowestIndexValidPartition
(
        void
)
{
    LwU32 pa_pos;
    LwU32 idx = 0;

    // Detect first PA that's up and running, we use a unicast read from that
    // partition to retrieve common values that can not be retrieved by a
    // broacast message
    gLowestIndexValidPartition = 0x00000000;
    LwU32 indexMask = 1;
    do
    {
        pa_pos = (gDisabledFBPAMask & indexMask);
        if (pa_pos)
        {
            gLowestIndexValidPartition += 0x4000;
        }
        idx++;
        indexMask = indexMask * 2;
    } while ((idx < MAX_PARTS) && pa_pos);
}




// Simple translator to get a unicast address based off the broadcast address
// and the partition index
LwU32
unicast
(
        LwU32 priv_addr,
        LwU32 partition
)
{
    LwU32 retval = priv_addr - 0x000a0000 + (partition * 0x00004000);
    return retval;
}


LwU32
detectPLLFrequencyMHz
(
        void
)
{
    LwU32 hbmpll_coeff = REG_RD32(BAR0, unicastMasterReadFromBroadcastRegister(LW_PFB_FBPA_FBIO_HBMPLL_COEFF));
    LwU32 mdiv = REF_VAL(LW_PFB_FBPA_FBIO_HBMPLL_COEFF_MDIV,hbmpll_coeff);
    LwU32 ndiv = REF_VAL(LW_PFB_FBPA_FBIO_HBMPLL_COEFF_NDIV,hbmpll_coeff);
    LwU32 pdiv = REF_VAL(LW_PFB_FBPA_FBIO_HBMPLL_COEFF_PLDIV,hbmpll_coeff);
    LwU32 freqMHz = (27 * ndiv) / (mdiv * pdiv);
    return freqMHz;
}


void
delay_us
(
        LwU32 delayInMicroSec
)
{
    // The input argument is the number of micro seconds to delay, need to assume
    // a worst case sysclk and program the timer based on that
    // Then round up the interval to guarantee minimum wait times observed

    LwU32 timer_interval = (delayInMicroSec) + 1;
    LwU32 timer_count;
    LwU32 timer_ctrl;

    // The colwersion from the free running 32ps setp timing normal to 1us ticks is continous
    // which means the delay could be close to 1us shorter than requested, to compensate we add
    // 1us to the requested value
    timer_interval++;

    REG_WR32(CSB, LW_CFBFALCON_TIMER_0_INTERVAL, timer_interval);

    // Set up timer for: Source = 1 us source, Mode = One-Shot
    timer_ctrl = REG_RD32(CSB, LW_CFBFALCON_TIMER_CTRL);
    timer_ctrl = FLD_SET_DRF(_CFBFALCON, _TIMER_CTRL, _SOURCE,_PTIMER_1US,timer_ctrl);
    timer_ctrl = FLD_SET_DRF(_CFBFALCON, _TIMER_CTRL, _MODE, _ONE_SHOT, timer_ctrl);

    REG_WR32(CSB, LW_CFBFALCON_TIMER_CTRL, timer_ctrl);

    // Start the timer
    timer_ctrl = FLD_SET_DRF(_CFBFALCON, _TIMER_CTRL, _GATE, _RUN, timer_ctrl);

    REG_WR32(CSB, LW_CFBFALCON_TIMER_CTRL, timer_ctrl);

    // Poll the timer counter until it expires
    timer_count = timer_interval;
    while (timer_count > 0)
    {
        timer_count = REG_RD32(CSB, LW_CFBFALCON_TIMER_0_COUNT);
    }

    // Stop the timer
    timer_ctrl = FLD_SET_DRF(_CFBFALCON, _TIMER_CTRL, _GATE, _STOP, timer_ctrl);

    REG_WR32(CSB, LW_CFBFALCON_TIMER_CTRL, timer_ctrl);

    return;
}

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GH10X))
//Helper functions for L2 slice disable
void read_l2_slice_disable_fuse(void)
{
    //Read the L2 slice disable fuse in groups of 2 PA. 
    //Bits [3:0] are for the even PA and [7:4] are for the odd PA
    LwU8 slice_idx = 0;
    LwU8 i;
    LwU8 pa_idx;
    for(pa_idx = 0 ; pa_idx < MAX_PARTS; pa_idx += 2) {
        LwU32 fuse_data = REG_RD32(BAR0, LW_FUSE_STATUS_OPT_L2SLICE_FBP((pa_idx>>1)));

        if (isPartitionEnabled(pa_idx)) {
            NIO_PRINT("PA%0d l2slice_dis = 0x%x", pa_idx, fuse_data & 0xF);
        }
        if (isPartitionEnabled(pa_idx+1)) {
            NIO_PRINT("PA%0d l2slice_dis = 0x%x", pa_idx + 1, (fuse_data >> 4) & 0xF);
        }

        //Save the 8 disable bits associated with these 2 PA
        for (i = 0; i < 2*CHANNELS_IN_PA; i++) {
            l2_slice_disabled[slice_idx++] = fuse_data & 0x1;
            fuse_data = fuse_data >> 1;
        }
    }
}

LwBool isL2SliceDisabled(
    LwU8  pa_idx,
    LwU8  subp_idx,
    LwU8  channel_idx
)
{
    LwU8 l2_slice_idx = (pa_idx * CHANNELS_IN_PA) + (subp_idx * CHANNELS_IN_SUBP) + channel_idx;
    if (l2_slice_idx < (MAX_PARTS*CHANNELS_IN_PA))
    {
        return l2_slice_disabled[l2_slice_idx];
    }
    return LW_TRUE;
}

LwBool isSubpDisabled(
    LwU8  pa_idx,
    LwU8  subp_idx
)
{
    //If both channels of the subp are disabled, then the entire subp is disabled
    LwU8 l2_slice_idx = (pa_idx * CHANNELS_IN_PA) + (subp_idx * CHANNELS_IN_SUBP);
    if (l2_slice_idx < (MAX_PARTS*CHANNELS_IN_PA - 2))
    {
        return (l2_slice_disabled[l2_slice_idx] && l2_slice_disabled[l2_slice_idx+1]);
    }
    return LW_FALSE;
}
#endif
