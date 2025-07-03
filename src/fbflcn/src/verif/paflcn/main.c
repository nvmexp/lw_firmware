/* _LWRM_COPYRIGHT_BEGIN_
 
 * Copyright 2016-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */


/*!
 * @file   main.c
 */


// location: ./tools/falcon-gcc/main/4.1.6/Linux/falcon-elf-gcc/lib/gcc/falcon-elf/4.3.2/include/falcon-intrinsics.h
//           ./tools/falcon-gcc/falcon6/6.4.0/Linux/falcon-elf-gcc/lib/gcc/falcon-elf/4.3.2/include/falcon-intrinsics.h

#include <falcon-intrinsics.h>

#include "rmfbflcncmdif.h"

#include "dev_fbpa.h"
#include "dev_pafalcon_csb.h"
#include "dev_fbfalcon_csb.h"
#include "dev_fuse.h"
#include "dev_gc6_island.h"

#include "osptimer.h"
#include "fbflcn_objfbflcn.h"
#include "fbflcn_defines.h"
#include "fbflcn_helpers.h"
#include "priv.h"

#include "csb.h"
#include "lwuproc.h"

//#include "config/fbfalcon-config.h"


// TODO: is there any real use for this revision ?
#define PAFALCON_UCODE_REVISION 0x01001001
const static LwU32 gREV = PAFALCON_UCODE_REVISION;

//OBJFBFLCN Fbflcn;

extern void *__stack_chk_guard;
extern LwU32 _spMin;
extern LwU32 _stack;


static void GCC_ATTRIB_NO_STACK_PROTECT()
canary_setup(void)
{
    // using 32 ns timer
    __stack_chk_guard = (void*) DRF_VAL(_CPAFALCON, _FALCON_PTIMER0, _VAL,
            CSB_REG_RD32(LW_CPAFALCON_FALCON_PTIMER0));
}

// Overrides libssp definition so falcon dumps some error info
extern void GCC_ATTRIB_NO_STACK_PROTECT()
_wrap____stack_chk_fail(void)
{
    FBFLCN_HALT(FBFLCN_ERROR_CODE_STACK_CANARY_CORRUPTED);
}


GCC_ATTRIB_NO_STACK_PROTECT()
int
main
(
        int argc,
        char **Argv
)
{
    // Setup SSP canary
    canary_setup();

    // Set SP_MIN
    CSB_REG_WR32(LW_CPAFALCON_FALCON_SP_MIN, DRF_VAL(_CPAFALCON, _FALCON_SP_MIN, _VALUE, (LwU32) &_spMin));

    FW_MBOX_WR32_STALL(2, gREV);
    FW_MBOX_WR32_STALL(2, gREV);    // dummy write for a force binary update - could be removed anytime in the future

    LwU32 rtnCode = LW_OK;
    do {
        // check code for added test above
        if (rtnCode != LW_OK)
        {
            FW_MBOX_WR32_STALL(13, rtnCode);
            FBFLCN_HALT(FBFLCN_ERROR_CODE_HALT_COMMAND);
        }
    } while (rtnCode == LW_OK);

    // stop code detected by the unit level environment for now, don't remove
    FBFLCN_HALT(FBFLCN_ERROR_CODE_REACHED_END_OF_MAIN);
    return 0;
}
