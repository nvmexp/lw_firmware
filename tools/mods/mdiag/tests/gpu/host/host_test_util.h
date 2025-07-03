#ifndef _HOST_TEST_UTIL_H
#define _HOST_TEST_UTIL_H

#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "lwmisc.h"
#include "mdiag/utils/randstrm.h"
#include "fermi/gf100/dev_flush.h"

enum host_test_field_t{
  CE_METHOD_OFFSET_IN_UPPER,
  CE_METHOD_OFFSET_OUT_UPPER,
  CE_METHOD_PITCH_IN,
  CE_METHOD_PITCH_OUT,
  CE_METHOD_LINE_LENGTH_IN,
  CE_METHOD_LINE_COUNT,
  CE_METHOD_SET_SEMAPHORE_A,
  CE_METHOD_LAUNCH_DMA
};

class host_test_util {
public:
    host_test_util(UINT32 copyclass);
    UINT32 host_test_dma_method(uint payloadSize64b=0);
    UINT32 host_test_field(host_test_field_t field);

private:
    UINT32 copyClassToUse;
};
#endif
