/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2015,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "mdiag/tests/gpu/2d/t5d.h"
#include "mdiag/tests/gpu/2d/verif2d/v2d.h"
#include "mdiag/tests/gpu/disp/vga_rma.h"
#include "mdiag/tests/gpu/disp/vstack.h"
#include "mdiag/tests/gpu/fb/basic_read_write.h"
#include "mdiag/tests/gpu/fb/enhanced_read_write.h"
#include "mdiag/tests/gpu/fb/host_all_size.h"
#include "mdiag/tests/gpu/fb/max_memory.h"
#include "mdiag/tests/gpu/fb/max_memory2.h"
#include "mdiag/tests/gpu/fb/sparsetex_copy_amap.h"
#include "mdiag/tests/gpu/fb/no_swizzle.h"
#include "mdiag/tests/gpu/fb/partition_white_box.h"
#include "mdiag/tests/gpu/fb/read_write_ordering.h"
#include "mdiag/tests/gpu/host/bl_remapper.h"
#include "mdiag/tests/gpu/host/fb_flush.h"
#include "mdiag/tests/gpu/host/frontdoor.h"
#include "mdiag/tests/gpu/host/host_bar1_perf.h"
#include "mdiag/tests/gpu/host/semaphore_bashing.h"
#include "mdiag/tests/gpu/mmu/vpr.h"
#include "mdiag/tests/gpu/trace_3d/trace_3d.h"
#include "mdiag/tests/gpu/misc/ad10x_jtag_reg_reset_check.h"
#include "mdiag/tests/gpu/misc/jtag_reg_reset_check.h"
#include "mdiag/tests/gpu/misc/failsafe_overt.h"
#include "mdiag/tests/gpu/misc/therm_overt_legacy.h"
#include "mdiag/tests/gpu/misc/therm_overt_latch.h"
#include "mdiag/tests/gpu/misc/xbashlwvdd_overt.h"
#include "mdiag/tests/gpu/fb/deadlock_cpu.h"
#include "mdiag/tests/gpu/fb/fb_hub_mmu_ilwalidate.h"
#include "mdiag/tests/gpu/fb/sysmembar_client_cpu.h"
#include "mdiag/tests/gpu/fb/sysmembar_l2_l2b_no_p2p.h"
#include "mdiag/tests/gpu/fb/sysmembar_l2_l2b.h"
#include "mdiag/tests/rtapi/rtapitest.h"
#include "mdiag/utl/utlusertest.h"
