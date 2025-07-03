/* _LWRM_COPYRIGHT_BEGIN_
 * *
 * * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * * information contained herein is proprietary and confidential to LWPU
 * * Corporation.  Any use, reproduction, or disclosure without the written
 * * permission of LWPU Corporation is prohibited.
 * *
 * * _LWRM_COPYRIGHT_END_
 * */

#ifndef SEC2_APMDEFINES_H
#define SEC2_APMDEFINES_H

/* ------------------------ System includes -------------------------------- */

// List of decode traps being used to enable APM protections
#define LW_SEC2_APM_DECODE_TRAP_SMC_MODE   (22)
#define LW_SEC2_APM_DECODE_TRAP_RS_MAP     (23)
#define LW_SEC2_APM_DECODE_TRAP_HV_CTL     (24)
#define LW_SEC2_APM_DECODE_TRAP_ENC_CFG    (29)

// Scratc index in GROUP0 to be used to communicate APM status
#define LW_SEC2_APM_SCRATCH_INDEX          (0x0)

// APM Status to be consumed by other modules within SC2
#define LW_SEC2_APM_STATUS_ILWALID   (0x0000DEAD)    
#define LW_SEC2_APM_STATUS_VALID     (0xBEEF0000)

// VBIOS Scratch registers being used for enablement state
#define LW_SEC2_APM_VBIOS_SCRATCH               LW_PGC6_AON_SELWRE_SCRATCH_GROUP_20
#define LW_SEC2_APM_VBIOS_SCRATCH_CCM           0:0 //CC Mode enable or disable
#define LW_SEC2_APM_VBIOS_SCRATCH_CCM_DISABLE   0x0
#define LW_SEC2_APM_VBIOS_SCRATCH_CCM_ENABLE    0x1

#define LW_SEC2_APM_VBIOS_SCRATCH_CCD           1:1 //CC Dev enable or disable (not applicable to APM)
#define LW_SEC2_APM_VBIOS_SCRATCH_CCD_DISABLE   0x0
#define LW_SEC2_APM_VBIOS_SCRATCH_CCD_ENABLE    0x1


#endif // SEC2_APMDEFINES_H
