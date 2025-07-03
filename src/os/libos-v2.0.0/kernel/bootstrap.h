/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#pragma once

typedef struct
{
    LwU64 imem_base_pa;             //! Physical address of ITCM region allocated to libos
    LwU64 imem_size;                //! Size in bytes of ITCM region allocated to libos
    LwU64 imem_bs_size;             //! Size in bytes of ITCM used by partition bootstrapper, from imem_base_pa
                                    //! This memory must be restored before a partition switch
    LwU64 dmem_base_pa;             //! Physical address of DTCM region allocated to libos
    LwU64 dmem_size;                //! Size in bytes of DTCM region allocated to libos
    LwU64 dmem_bs_size;             //! Size in bytes of DTCM used by partition bootstrapper, from dmem_base_pa
                                    //! This memory must be restored before a partition switch
} libos_bootstrap_args_t;

typedef __attribute__((noreturn)) void (*kernel_bootstrap_t)(libos_bootstrap_args_t *args);
