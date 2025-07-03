/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PR_COMMON_H
#define PR_COMMON_H

#include <lwtypes.h>

#define PR_ATTR_DATA_OVLY(data)         GCC_ATTRIB_SECTION("dmem_pr", #data)
#define PR_SHARED_ATTR_DATA_OVLY(data)  GCC_ATTRIB_SECTION("dmem_prShared", #data)

/*!
 * @brief For Playready, we have a shared FB region between LWDEC and SEC2 to share info selwrely,
 *        In this region, we have a struct SHARED_DATA_OFFSETS which points to the actual data and the actual data itself
 *        This enumeration specifies whether to share start address of struct or the actual data
 *        It is used as a argument to prGetSharedDataRegionDetails*()
 */
typedef enum _shared_region_start_option
{
    PR_GET_SHARED_STRUCT_START,
    PR_GET_SHARED_DATA_START,
} SHARED_REGION_START_OPTION;

#endif // PR_COMMON_H
