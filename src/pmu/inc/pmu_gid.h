/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */


#ifndef PMU_GID_H
#define PMU_GID_H

/*!
 * @file    pmu_gid.h
 * @brief   Compute a GPU ID, using the ECID as a seed and SHA-1 as the
 *          hash algorithm.
 */

void
pmuGenerateGid(void);

#endif //PMU_GID_H
