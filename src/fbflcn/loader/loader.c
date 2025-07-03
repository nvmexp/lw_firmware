/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------- System Includes ------------------------------- */
#include <falcon-intrinsics.h>
#include <stddef.h>

/* ------------------------- Application Includes -------------------------- */
#include "lwmisc.h"
#include "rmflcnbl.h"

/* ------------------------- Defines --------------------------------------- */

/*!
 * Falcon DMA requests are issued with respect to a specific index/aperture in
 * the Falcon's Frame Buffer Interface.  The index is stored in the CTX Special
 * Purpose Register (SPR) and duplicated per DMA operation type (dmread,
 * dmwrite, imread). Tesla refers to this index as the ctxdma index and Fermi
 * refers to it as the transaction-type. Software just refers to its as the DMA
 * index.
 */
#define  CTX_DMAIDX_SHIFT_IMREAD            (0x00)
#define  CTX_DMAIDX_SHIFT_DMREAD            (0x08)
#define  CTX_DMAIDX_SHIFT_DMWRITE           (0x0c)

/* ------------------------- Types Definitions ----------------------------- */
typedef int (*ENTRY_FN)(int argc, char **argv);

/* ------------------------- Global Variables ------------------------------ */

/*!
 * This stucture should be loaded in DMEM (by the simulator or RM) from a given
 * application image, the tools mkloadimage can generate the DMEM structure
 * LOADER_CONFIG
 */
LOADER_CONFIG LoaderConfig;

/* ------------------------- Prototypes ------------------------------------ */
void *memcpy(void *s1, const void *s2, size_t n);

/* ------------------------- Implementation -------------------------------- */

int
main
(
    int   argc,
    char *argv[]
)
{
    LwU32 srcOffs  = 0;
    LwU32 destOffs = 0;
    LOADER_CONFIG config;

    //
    // Copy the bootloader configuration data onto the stack. This is necessary
    // to prevent this bootloader from overwriting its configuration (at DMEM
    // address 0x00) when it copies the application's data segment (.data) into
    // DMEM (typically targeted for DMEM address 0x00).  The assumption here
    // is that stack pointer is high enough in memory that it will never
    // collide with the application's .data section.
    //
    memcpy(&config, &LoaderConfig, sizeof(LOADER_CONFIG));

    // setup the DMA indexes for all DMA operation types
    falc_wspr(CTX, (config.dmaIdx << CTX_DMAIDX_SHIFT_IMREAD |
                    config.dmaIdx << CTX_DMAIDX_SHIFT_DMREAD |
                    config.dmaIdx << CTX_DMAIDX_SHIFT_DMWRITE));

    // load IMEM
    falc_wspr(IMB, ((config.codeDmaBase.lo >> 8) |
                    (config.codeDmaBase.hi << 24)));
#if PA_47_BIT_SUPPORTED
    falc_wspr(IMB1, (config.codeDmaBase.hi >> 8));
#endif
    srcOffs = config.codeEntryPoint;
    while (destOffs < config.codeSizeToLoad)
    {
        falc_imread(srcOffs, destOffs);
        srcOffs  += 256;
        destOffs += 256;
    }
    falc_imwait();

    // load DMEM
    falc_wspr(DMB, ((config.dataDmaBase.lo >> 8) |
                    (config.dataDmaBase.hi << 24)));
#if PA_47_BIT_SUPPORTED
    falc_wspr(DMB1, (config.dataDmaBase.hi >> 8));
#endif

    srcOffs = 0;
    while (srcOffs < config.dataSize)
    {
        //
        // bits 18:16 of second operand of dmread defines the actual size of
        // transfer (chunk of 256)
        //
        falc_dmread(srcOffs, srcOffs + 0x60000);
        srcOffs += 256;
    }

    falc_dmwait();

    // now jump to the entry point
    ((ENTRY_FN)(config.codeEntryPoint))(config.argc, (char **)config.argv);
    return 0;
}

void *memcpy(void *s1, const void *s2, size_t n)
{
    size_t index;
    for (index = 0; index < n; index++)
    {
        ((LwU8*)s1)[index] = ((LwU8*)s2)[index];
    }
    return s1;
}

