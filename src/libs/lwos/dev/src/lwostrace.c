/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    lwostrace.c
 * @copydoc lwostrace.h
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwostrace.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
#define PMU_DEBUG_3 0x5cc
static LwU32 seq = 0;
static LwU32 dbgRegAddress = PMU_DEBUG_3;

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */

#define OS_TRACE_LOG2_SIZE 6

static LwU32 traceCtx;
// assign a section to trace_buffer (otherwise the alignement is not respected on falcon-elf-gcc )
static LwU32 traceBuffer[(1 << OS_TRACE_LOG2_SIZE) / 4]
    __attribute__ ((aligned(1 << OS_TRACE_LOG2_SIZE)))
    __attribute__ ((section(".data")));
static LwU32 traceStartOffset;  // Upper 32-bit of trace in FB
static LwU32 traceSize;         // It must be a multiple of TRACE_LOG2_SIZE ^ 2
static LwU32 traceOffset;       // Current offset to add to trace_start_offset where to write current trace
static LwU32 traceCounter;

void
osTraceRegInit
(
    LwU32 addr
)
{
    dbgRegAddress = addr;
}

void
osTraceReg
(
    char   *str,
    LwU32   param1,
    LwU32   param2,
    LwU32   param3,
    LwU32   param4
)
{
    LwU32   csw;
    LwU32   i;
    LwU32   val;
    char   *ptr  = str;
    LwU32   done = 0;

    falc_rspr( csw, CSW );
    falc_wspr( CSW, csw & ~( 3 << 16 )); // mask interrupts

    val = ( (0xbeef<<16) | seq ); // start of a message
    falc_stx((unsigned int*)dbgRegAddress, val);

    falc_stx((unsigned int*)dbgRegAddress, param1);
    falc_stx((unsigned int*)dbgRegAddress, param2);
    falc_stx((unsigned int*)dbgRegAddress, param3);
    falc_stx((unsigned int*)dbgRegAddress, param4);

    while(!done) {
        val=0;
        for(i=0;i<4;i++) {
            if(*ptr != '\0')
            {
                 val |= ( (*ptr)<< (i<<3) );
            }
            else {
                done = 1;
                break;
            }
            ptr++;
        }
        falc_stx((unsigned int*)dbgRegAddress, val);
    }

    seq++;

    falc_wspr( CSW, csw);
}

void
osTraceFbInit
(
    RM_FLCN_MEM_DESC *pBufDesc
)
{
    traceStartOffset =
        (pBufDesc->address.lo >> 8) | (pBufDesc->address.hi << 24);
    traceSize = DRF_VAL(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, pBufDesc->params);
    traceCtx = DRF_VAL(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX, pBufDesc->params);
}

void
osTraceFb
(
    const char *fmt,
    LwU32       arg1,
    LwU32       arg2,
    LwU32       arg3,
    LwU32       arg4
)
{
    LwU32 csw;
    LwU32 dmb;
    LwU32 ctx;
    LwU32 i = 0;
    char  *str, *traceStr;
    falc_rspr( csw, CSW );
    falc_wspr( CSW, csw & ~( 3 << 16 )); // mask interrupts
    falc_rspr( ctx, CTX );
    falc_wspr( CTX, (ctx & ~(7 << 12)) | (traceCtx << 12) ); //setup CTX DMA write
    falc_rspr( dmb, DMB );
    falc_wspr( DMB, traceStartOffset);
    falc_dmwait();
    traceBuffer[0] = traceCounter++;
    traceBuffer[1] = arg1;
    traceBuffer[2] = arg2;
    traceBuffer[3] = arg3;
    traceBuffer[4] = arg4;
    str = (char  *)fmt;
    traceStr = (char  *)&traceBuffer[5];

    do {
       traceStr[i] = str[i];
       i++;
    } while ((i <  ((1 << OS_TRACE_LOG2_SIZE) - 5*sizeof(int) -1)) && (str[i] != '\0'));

    traceStr[i] = '\0';
    falc_dmwrite( traceOffset, ((OS_TRACE_LOG2_SIZE - 2) << 16) | (LwU32)traceBuffer );
    traceOffset += 1 << OS_TRACE_LOG2_SIZE;

    if (traceOffset >= traceSize)
    {
        traceOffset = 0;
    }

    //restore SPRs
    falc_wspr( DMB, dmb);
    falc_wspr( CTX, ctx);
    falc_wspr( CSW, csw);
}
