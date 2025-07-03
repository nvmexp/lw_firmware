/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file       c_cover_io_wrapper.c
 *
 * @details    This file builds on top of vectorcast provided c_cover_io.c,
 *             which has exposure to vcast_c_options.h (which can only be
 *             included once, hence the wrapper.)
 */

#include "../vcast_elw/c_cover_io.c"
#include "lwuproc.h"
#include <sections.h>
#include <lwriscv/print.h>

sysSHARED_DATA static const struct vcast_unit_list * vcDataOffset = vcast_unit_list_values;

/*
 * @brief      Print VCAST data
 */
void sysSHARED_CODE vcastPrintData(void)
{
    char const hex[17] = "0123456789abcdef";
    LwU32 idx;

    sysPrivilegeRaise();
    for (; vcDataOffset->vcast_unit; vcDataOffset++) 
    {
        for (idx = 0; idx < vcDataOffset->vcast_statement_bytes; idx += 1)
        {
            putchar(hex[vcDataOffset->vcast_statement_array[idx] >> 8]);
            putchar(hex[vcDataOffset->vcast_statement_array[idx] & 0xF]);
        }
        for (idx = 0; idx < vcDataOffset->vcast_branch_bytes; idx += 1)
        {
            putchar(hex[vcDataOffset->vcast_branch_array[idx] >> 8]);
            putchar(hex[vcDataOffset->vcast_branch_array[idx] & 0xF]);
        }
        // TODO not sure why these breaks are needed
        putchar('\n');
    }
    putchar('\n');
    sysPrivilegeLower();

    vcDataOffset = vcast_unit_list_values;
}
