#include "rmt_dmemva_test_cases.h"

#include "lwmisc.h"
#include "core/include/lwrm.h"
#include "core/include/gpu.h"

const FalconInfo *FalconInfo::getVoltaFalconInfo(enum FALCON_INSTANCE instance)
{
    switch (instance) {
    case FalconInfo::FALCON_GSP:
        return FalconInfo::getTuringFalconInfo(instance);
    case FalconInfo::FALCON_SEC2:
    case FalconInfo::FALCON_PMU:
    case FalconInfo::FALCON_LWDEC:
        return FalconInfo::getPascalFalconInfo(instance);
    }

    return NULL;
}
