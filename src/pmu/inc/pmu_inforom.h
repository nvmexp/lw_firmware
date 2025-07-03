/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_inforom.h
 * @brief   Contains inforom specific defines.
 */

#ifndef PMU_INFOROM_H
#define PMU_INFOROM_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
/* ------------------------- Types Definitions ----------------------------- */
typedef struct INFOROM_REGION INFOROM_REGION;

/*!
 * Inforom is a writeable region in ROM.
 */
struct INFOROM_REGION
{
    LwU32    startAddress;
    LwU32    endAddress;
};

/* ------------------------ External Definitions --------------------------- */
extern INFOROM_REGION inforom;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
#define LW_INFOROM_IMAGE_SIG_SIZE   (4)
#define LW_INFOROM_IMAGE_SIG        (0x5346464A)

/* ------------------------ Function Prototypes ---------------------------- */
#endif // PMU_INFOROM_H
