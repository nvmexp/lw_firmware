/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */


#include <falcon-intrinsics.h>
#include "fbflcn_defines.h"
#include "config/fbfalcon-config.h"
#include "cpu-intrinsics.h"
#include "csb.h"


#ifndef SEGMENT_H_
#define SEGMENT_H_

typedef enum SEGMENT_NAME { INIT_SEGMENT=0, MCLK_SEGMENT=1, TEMP_SEGMENT=2, PAFLCN_SEGMENT=3, SEGMENT_COUNT=4 } SegmentName_t;
typedef enum SEGMENT_FIELD {VIRTUAL_ADDR=0, SIZE=1, PHYS_ADDR=2, IDENTIFIER=3, FIELD_COUNT=4 } SegmentField_t;
typedef enum LOCATION_NAME {SEQUENCER_INFO=0, SEQUENCER_DATA=1, LOCATION_NAME_FIELD_COUNT=2 } LocationName_t;

// overlay table is defined as data inserts in the linker script
extern LwU32 _ovly_table[SEGMENT_COUNT][FIELD_COUNT];
extern LwU32 _pafalcon_location_table[LOCATION_NAME_FIELD_COUNT];

void loadSegment(SegmentName_t)
    GCC_ATTRIB_SECTION("resident", "readSegment");


#endif /* SEGMENT_H_ */
