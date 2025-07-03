/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: entry.c
 * @brief: Entry point to GSP HS applets, this is NS bootstrapper
 */

//
// Includes
//
#include <dev_gsp_csb.h>
#include "gsphs.h"

//
// The new SVEC field definition is ((num_blks << 24) | (e << 17) | (s << 16) | tag_val)
// assuming size is always multiple of 256, then (size << 16) equals (num_blks << 24)
//
#define SVEC_REG(base, size, s, e)  (((base) >> 8) | (((size) | (s)) << 16) | ((e) << 17))

//
// Global Variables
//
LwU8 g_signature[16]
    GCC_ATTRIB_ALIGN(16)
    GCC_ATTRIB_SECTION("data", "g_signature");

//
// Linker script variables holding the start and end
// of secure overlay.
//
extern LwU8 _hs_ovl_start[];
extern LwU8 _hs_ovl_size[];

#define TC_INFINITY    (0x001f)
static void falc_scp_trap(LwU32 tc)
{
    asm volatile("cci %0;" : : "i" (tc));
}

static void initHs(void)
{
    LwU32 start = ((LwU32)_hs_ovl_start & 0x0FFFFFFF);
    LwU32 size  = ((LwU32)_hs_ovl_size & 0x0FFFFFFF);

    // Load the signature first
    falc_scp_trap(TC_INFINITY);
    falc_dmwrite(0, ((6 << 16) | (LwU32)(g_signature)));
    falc_dmwait();
    falc_scp_trap(0);

    //
    // All the required code is already loaded into IMEM
    // Just need to program the SEC spr and jump into code
    //
    falc_wspr(SEC, SVEC_REG(start, size, 0, 1));
}

int main(int argc, char **Argv)
{
    static LwBool bHsInitialized = LW_FALSE;
    LwU32 cmd;
    LwU32 arg;

    if (!bHsInitialized)
    {
        initHs();
        bHsInitialized = LW_TRUE;
    }

    while (LW_TRUE)
    {
         //
         // Temporary HS call protocol: MBOX(0) is command and return others are
         // command-specific.
         //
        cmd = csbRd32(LW_CGSP_MAILBOX(0));
        arg = csbRd32(LW_CGSP_MAILBOX(1));

        //
        // This is HS switch
        //
        if (cmd >= GSPHS_CMD_RANGE_HS_START && cmd <= GSPHS_CMD_RANGE_HS_END)
        {
            // Call into HS for others
            enterHs(cmd, arg);
        }
        else
        {
            // Ordinary command
            switch(cmd)
            {
            case GSPHS_CMD_TIME_CORE_SWITCH: // time core switch
            {
                LwU32 lo, hi;
                do
                {
                    hi = csbRd32(LW_CGSP_FALCON_PTIMER1);
                    lo = csbRd32(LW_CGSP_FALCON_PTIMER0);
                } while (hi != csbRd32(LW_CGSP_FALCON_PTIMER1));

                csbWr32(LW_CGSP_MAILBOX(0), FLCN_OK);
                csbWr32(LW_CGSP_MAILBOX(1), lo);
                csbWr32(LW_CGSP_MAILBOX(2), hi);
                break;
            }
            default:
                csbWr32(LW_CGSP_MAILBOX(0), FLCN_ERR_ILWALID_ARGUMENT);
                break;
            }
        }

        //
        // This is known (by ASIC) bug - we need to have at least one NS
        // instruction before switching back to RISC-V.
        //
        asm volatile("nop;");

        //
        // Switch back to RISC-V
        //
        falc_wait(0xC6);
    }
    return 0;
}
