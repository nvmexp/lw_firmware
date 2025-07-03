/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_selwrity.h
 */

#ifndef PMU_SELWRITY_H
#define PMU_SELWRITY_H

/* ------------------------ System includes --------------------------------- */
/* ------------------------ Application includes ---------------------------- */
/* ------------------------ Types definitions ------------------------------- */
/* ------------------------ External definitions ---------------------------- */
/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */

/*!
 * In order to support PMU API protection, we use a priv-protected scratch register.
 * This register, is normally written by VBIOS. The defines below are used to
 * interpret the fields of this register. VSB stands for: Vbios Security Bypass.
 */
#define LW_VSB_REG0_TEMP_SIM                                                 0:0
#define LW_VSB_REG0_TEMP_SIM_DISALLOWED                               0x00000000
#define LW_VSB_REG0_TEMP_SIM_ALLOWED                                  0x00000001

#define LW_VSB_REG0_I2C_WRITES                                               1:1
#define LW_VSB_REG0_I2C_WRITES_DISALLOWED                             0x00000000
#define LW_VSB_REG0_I2C_WRITES_ALLOWED                                0x00000001

#define LW_VSB_REG0_RM_TEST_EXELWTE                                          2:2
#define LW_VSB_REG0_RM_TEST_EXELWTE_NO                                0x00000000
#define LW_VSB_REG0_RM_TEST_EXELWTE_YES                               0x00000001

#define LW_VSB_REG0_UCODE_PROFILING                                          3:3
#define LW_VSB_REG0_UCODE_PROFILING_DISALLOWED                        0x00000000
#define LW_VSB_REG0_UCODE_PROFILING_ALLOWED                           0x00000001

#define LW_VSB_REG0_POWER_CONTROLLER_LOCK                                    4:4
#define LW_VSB_REG0_POWER_CONTROLLER_LOCK_ENABLED                     0x00000000
#define LW_VSB_REG0_POWER_CONTROLLER_LOCK_DISABLED                    0x00000001

#define LW_VSB_REG0_SMBPBI_DEBUG                                             5:5    /* RWIVF */
#define LW_VSB_REG0_SMBPBI_DEBUG_DISABLED                             0x00000000    /* RWIV- */
#define LW_VSB_REG0_SMBPBI_DEBUG_ENABLED                              0x00000001    /* RW-V- */

#define LW_VSB_REG0_IFR_HULK_STATUS                                        20:20
#define LW_VSB_REG0_IFR_HULK_STATUS_ILWALID                           0x00000000
#define LW_VSB_REG0_IFR_HULK_STATUS_VALID                             0x00000001

#define LW_VSB_REG0_ACR_HS_BIN_ALLOW_REV_1_HW                              21:21
#define LW_VSB_REG0_ACR_HS_BIN_ALLOW_REV_1_HW_NO                      0x00000000
#define LW_VSB_REG0_ACR_HS_BIN_ALLOW_REV_1_HW_YES                     0x00000001

#define LW_VSB_REG0_IFR_CERT_VERIF_EXELWTED                                22:22
#define LW_VSB_REG0_IFR_CERT_VERIF_EXELWTED_NO                        0x00000000
#define LW_VSB_REG0_IFR_CERT_VERIF_EXELWTED_YES                       0x00000001

#define LW_VSB_REG0_IFR_CERT_VERIF_RESULT                                  23:23
#define LW_VSB_REG0_IFR_CERT_VERIF_RESULT_FAILURE                     0x00000000
#define LW_VSB_REG0_IFR_CERT_VERIF_RESULT_SUCCESS                     0x00000001

#define LW_VSB_REG0_OVERT_TEMP_THRESHOLD                                   31:24
#define LW_VSB_REG0_OVERT_TEMP_THRESHOLD_ILWALID                      0x00000000
#define LW_VSB_REG0_OVERT_TEMP_THRESHOLD_MIN                          0x00000001
#define LW_VSB_REG0_OVERT_TEMP_THRESHOLD_MAX                          0x000000ff

/* ------------------------ Public Functions -------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Work-Item IMPL Prototypes ----------------------- */

#endif // PMU_SELWRITY_H

