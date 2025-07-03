/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <falcon-intrinsics.h>

#include "fbflcn_helpers.h"
#include "fbflcn_defines.h"
#include "priv.h"

// include manuals
#include "dev_fbfalcon_csb.h"
#include "dev_fbpa.h"


#include "fbflcn_access.h"
#include "fbflcn_table.h"

#include "osptimer.h"
#include "memory.h"
#include "config/g_memory_hal.h"


void
memoryInitHbmChannelConfig_GH100
(
        void
)
{

    // Helper functions for slice and channel mapping on HBM memories
    static const LwU8 pa_map_GH100[MAX_PARTS] = {0,1,4,5, 2,3,8,9, 6,7,10,11, 14,15,18,19, 12,13,22,23, 16,17,20,21};  // vbios order site B & E diff
    static const LwU8 slice2hbmChannelMap_GH100[24][4] = {
      //      HBM3
      {6,  2,  7,  3 }, //FBPA0
      {14, 10, 15, 11}, //FBPA1
      {0,  4,  1,  5 }, //FBPA2
      {8,  12, 9,  13}, //FBPA3
      {0,  4,  1,  5 }, //FBPA4
      {8,  12, 9,  13}, //FBPA5
      {3,  7,  2,  6 }, //FBPA6
      {11, 15, 10, 14}, //FBPA7
      {6,  2,  7,  3 }, //FBPA8
      {14, 10, 15, 11}, //FBPA9
      {5,  1,  4,  0 }, //FBPA10
      {13, 9,  12, 8 }, //FBPA11
      {5,  1,  4,  0 }, //FBPA12
      {13, 9,  12, 8 }, //FBPA13
      {6,  2,  7,  3 }, //FBPA14
      {14, 10, 15, 11}, //FBPA15
      {3,  7,  2,  6 }, //FBPA16
      {11, 15, 10, 14}, //FBPA17
      {0,  4,  1,  5 }, //FBPA18
      {8,  12, 9,  13}, //FBPA19
      {5,  1,  4,  0 }, //FBPA20
      {13, 9,  12, 8 }, //FBPA21
      {3,  7,  2,  6 }, //FBPA22
      {11, 15, 10, 14}  //FBPA23
    };

    LwU8 i;                 // iterator for pa per site [0:PARTS_PER_SITE-1]
    LwU8 s;                 // site
    LwU8 p;                 // partition
    LwU8 spa_idx;           // site pa index (multiples of PARTS_PER_SITE)
    LwU8 c_idx;             // channel index per PA
    LwU8 num_valid_pa;

    // init l2_slice_disabled array
    read_l2_slice_disable_fuse();

    // loop through all sites
    for (s=0; s < MAX_SITES; s++)
    {
        // init struct
        hbm_bcast_data[s].enabled = LW_FALSE;
        hbm_bcast_data[s].valid_channel = 0xFF;

        // find all valid partitions per site
        num_valid_pa = 0;
        spa_idx = s * PARTS_PER_SITE;      // PARTS_PER_SITE = 4
        for (i=0; i < PARTS_PER_SITE; i++)
        {
            p = pa_map_GH100[spa_idx++];          // colwert to FPBA# (eg site=1, spa_idx=4..7, p=8,9,2,3)
            if (isPartitionEnabled(p)) {
                hbm_bcast_data[s].pa[num_valid_pa] = p;
                num_valid_pa++;

                // find first valid channel in site
                if (hbm_bcast_data[s].valid_channel == 0xFF)
                {
                    for (c_idx=0; c_idx < CHANNELS_IN_PA; c_idx++)
                    {
                        if (!isL2SliceDisabled(p, ((c_idx >> 1) & 0x1), (c_idx & 0x1)))       //p(24),subp(2),c(2)
                        {
                            hbm_bcast_data[s].valid_channel = slice2hbmChannelMap_GH100[p][c_idx];
                            NIO_PRINT("HBM_PER_TR_SEQ -  valid_channel = %d channel_id = %d\n", hbm_bcast_data[s].valid_channel, c_idx);
                            break;
                        }
                    }
                }
            }
        }

        hbm_bcast_data[s].numValidPa = num_valid_pa;
        if (num_valid_pa != 0)
        {
            NIO_PRINT("HBM_PER_TR_SEQ - hbmSiteEnabled[%d] = true\n", s);
            hbm_bcast_data[s].enabled = LW_TRUE;
        }
    }
}

