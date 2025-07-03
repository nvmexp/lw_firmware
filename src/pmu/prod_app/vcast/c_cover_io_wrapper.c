/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
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

#include "lwuproc.h"
#include "../vcast_elw/c_cover_io.c"
#include "pmusw.h"

int vcastGetDataLength(void)
    GCC_ATTRIB_SECTION("imem_resident", "vcastGetDataLength");
LwBool vcastCopyData(LwU8 * pDest, LwU32 * pSize)
    GCC_ATTRIB_SECTION("imem_resident", "vcastCopyData");
#if (PMUCFG_FEATURE_ENABLED(PMU_VC_RAW_DATA))
static const struct vcast_unit_list * vcDataOffset = vcast_unit_list_values;
#else
LwU32 bytesCopied = 0;
#endif

/**
 * @brief      Get the length in bytes of the code coverage data.
 *
 * @details    The function assumes for statement and branch coverage are turned
 *             on.
 *
 * @return     Returns the length in bytes of the code coverage data.
 */
int vcastGetDataLength(void)
{
    const struct vcast_unit_list * vcast_unit_lwr;
    int numBytes = 0;
#ifdef VCAST_COVERAGE_TYPE_STATEMENT_BRANCH
    for (vcast_unit_lwr = vcast_unit_list_values;
         vcast_unit_lwr->vcast_unit;
         vcast_unit_lwr++) 
    {
        numBytes += vcast_unit_lwr->vcast_statement_bytes;
        numBytes += vcast_unit_lwr->vcast_branch_bytes;
    }
    return numBytes;
#endif
}

/*
 * @brief      Reset Vcast buffers
 *
 * @details    This function is used to call an internal vectorcast function
 *             to reset all coverage data buffers.
 */
void vcastResetData()
{
#if (PMUCFG_FEATURE_ENABLED(PMU_VC_RAW_DATA))
    VCAST_CLEAR_COVERAGE_DATA_ID();
#else
    VCAST_CLEAR_COVERAGE_DATA();
#endif
}

/*
 * @brief      Copy VCAST data to RPC buffer
 *
 * @details    The function copies the vcast data upto the input buffer size
 *             from pmu to the RPC buffer provided
 *
 * @return     Returns if all data has been copied or furthur RPC calls are
 *             necessary
 */
LwBool vcastCopyData(LwU8 * pDest, LwU32 * pSize)
{
    LwU32 numBytes = 0;
#if (PMUCFG_FEATURE_ENABLED(PMU_VC_RAW_DATA))
    for (; vcDataOffset->vcast_unit; vcDataOffset++) 
    {
        numBytes += vcDataOffset->vcast_statement_bytes;
        numBytes += vcDataOffset->vcast_branch_bytes;
        if (numBytes > *pSize)
        {
            // Update size
            *pSize = numBytes - vcDataOffset->vcast_statement_bytes - vcDataOffset->vcast_branch_bytes;
            return LW_FALSE;
        }

        memcpy(pDest, vcDataOffset->vcast_statement_array, vcDataOffset->vcast_statement_bytes);
        pDest += vcDataOffset->vcast_statement_bytes;
        memcpy(pDest, vcDataOffset->vcast_branch_array, vcDataOffset->vcast_branch_bytes);
        pDest += vcDataOffset->vcast_branch_bytes;
    }
    // Update final size
    *pSize = numBytes;
    // Re-init iterator
    vcDataOffset = vcast_unit_list_values;
    return LW_TRUE;
#else
    // Dump coverage data using vectorcast methods/variables if RPC is called for the first time
    if (bytesCopied == 0)
    {
        vcast_ascii_coverage_data_pool[0] = 0;
        vcast_ascii_coverage_data_pos = 0;
        vcast_coverage_data_buffer_full = 0;
        VCAST_DUMP_COVERAGE_DATA();
    }

    numBytes = sizeof(vcast_ascii_coverage_data_pool);
    //
    // Copy the data to buffer. If there is enough space for all the data, copy and return LW_TRUE.
    // If more RPC calls are required to copy the data, copy upto pSize and return LW_FALSE.
    //
    if (bytesCopied + *pSize > numBytes)
    {
        memcpy(pDest, (vcast_ascii_coverage_data_pool + bytesCopied), (numBytes - bytesCopied));
        *pSize = numBytes - bytesCopied;
        bytesCopied = 0;
        return LW_TRUE;
    }
    else
    {
        memcpy(pDest, (vcast_ascii_coverage_data_pool + bytesCopied), *pSize);
        bytesCopied+= *pSize;
        return LW_FALSE;
    }
#endif
}
