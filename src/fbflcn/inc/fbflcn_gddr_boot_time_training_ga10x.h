/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef FBFLCN_GDDR_BOOT_TIME_TRAINING_GA10X
#define FBFLCN_GDDR_BOOT_TIME_TRAINING_GA10X

#include "fbflcn_defines.h"
#include "memory.h"

extern LwBool gbl_en_fb_mclk_sw;

// MRS register fields define
//#define LW_PFB_FBPA_GENERIC_MRS_ADR_HBM_RDBI    0:0

//Define functions here that need to be used globally
void func_setup_g6_addr_tr (void);

LwU32 doBootTimeTraining (void);

// export functions used in the training data loading and recovery in the mclk switch binary
LwU32 unicast_wr_vref_dfe (
        LwU32 priv_addr,
        LwU32 partition,
        LwU32 subp,
        LwU32 byte);
void func_program_hybrid_opt_in_fbio (
        LwU32 opt_hybrid_dq[TOTAL_DQ_BITS],
        LwU32 opt_hybrid_dbi[TOTAL_DBI_BITS],
        LwU32 opt_hybrid_edc[TOTAL_DBI_BITS],
        LwU32 edc_en);
void func_program_optimal_vref_fbio(
        LwU32 opt_hybrid_dq[TOTAL_DQ_BITS],
        LwU32 opt_hybrid_dbi[TOTAL_DBI_BITS],
        LwU32 opt_hybrid_edc[TOTAL_DBI_BITS],
        LwU32 edc_en);
void func_program_hybrid_opt_in_dram(
        LwU32 opt_hybrid_dq[32],
        LwU32 dq,
        LwU32 dbi,
        LwS8 offset);
void func_program_vref_in_dram(
        LwU32 opt_hybrid_dq[0],
        LwU32 dq,
        LwU32 dbi,
        LwS8 offset);


#endif // FBFLCN_GDDR_BOOT_TIME_TRAINING_GA10X
