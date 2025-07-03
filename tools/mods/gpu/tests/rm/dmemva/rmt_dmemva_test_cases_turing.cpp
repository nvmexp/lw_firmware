#include "rmt_dmemva_test_cases.h"

#include "lwmisc.h"
#include "core/include/lwrm.h"
#include "core/include/gpu.h"
#include "gpu/include/gpusbdev.h"

#include "turing/tu102/dev_falcon_v4.h"
#include "turing/tu102/dev_sec_pri.h"
#include "turing/tu102/dev_gsp.h"
#include "turing/tu102/dev_master.h"

#include "gpu/tests/rm/dmemva/ucode/g_dmvauc_sec2_tu10x.h"
#include "gpu/tests/rm/dmemva/ucode/g_dmvauc_gsp_tu10x.h"
#include "gpu/tests/rm/dmemva/ucode/g_dmvauc_sec2_tu10x_gp102_sig.h"
#include "gpu/tests/rm/dmemva/ucode/g_dmvauc_gsp_tu10x_gp102_sig.h"

class TuringSecInfo : public FalconInfo
{
   public:

    virtual FALCON_INSTANCE getEngineId(void) const
    {
        return FALCON_SEC2;
    }

    virtual const char *getFalconName(void) const
    {
        return "SEC2";
    }

    virtual LwU32 getEngineBase(void) const
    {
        return LW_PSEC_FALCON_IRQSSET;
    }

    virtual LwU32 *getUcodeData(void) const
    {
        return dmva_ucode_data_sec2_tu10x;
    }

    virtual LwU32 *getUcodeHeader(void) const
    {
        return dmva_ucode_header_sec2_tu10x;
    }

    virtual LwU32 getPatchLocation(void) const
    {
        return dmva_sec2_sig_patch_location[0];
    }

    virtual LwU32 getPatchSignature(void) const
    {
        return dmva_sec2_sig_patch_signature[0];
    }

    virtual LwU32 *getSignature(bool debug = true) const
    {
        if (debug)
            return dmva_sec2_sig_dbg;
        else
            return dmva_sec2_sig_prod;
    }

    virtual LwU32 setPMC(LwU32 lwPmc, LwU32 state) const
    {
        return FLD_SET_DRF_NUM(_PMC, _ENABLE, _SEC, state, lwPmc);
    }

    virtual void engineReset(GpuSubdevice* pSubdev) const
    {
        pSubdev->RegWr32(LW_PSEC_FALCON_ENGINE, REF_DEF(LW_PSEC_FALCON_ENGINE_RESET, _TRUE));
        pSubdev->RegWr32(LW_PSEC_FALCON_ENGINE, REF_DEF(LW_PSEC_FALCON_ENGINE_RESET, _FALSE));
    }

    virtual ~TuringSecInfo(void)
    {
    }
};

class TuringGspInfo : public FalconInfo
{
   public:

    virtual FALCON_INSTANCE getEngineId(void) const
    {
        return FALCON_GSP;
    }

    virtual const char *getFalconName(void) const
    {
        return "GSP";
    }

    virtual LwU32 getEngineBase(void) const
    {
        return LW_FALCON_GSP_BASE;
    }

    virtual LwU32 *getUcodeData(void) const
    {
        return dmva_ucode_data_gsp_tu10x;
    }

    virtual LwU32 *getUcodeHeader(void) const
    {
        return dmva_ucode_header_gsp_tu10x;
    }

    virtual LwU32 getPatchLocation(void) const
    {
        return dmva_gsp_sig_patch_location[0];
    }

    virtual LwU32 getPatchSignature(void) const
    {
        return dmva_gsp_sig_patch_signature[0];
    }

    virtual LwU32 *getSignature(bool debug = true) const
    {
        if (debug)
            return dmva_gsp_sig_dbg;
        else
            return dmva_gsp_sig_prod;
    }

    virtual LwU32 setPMC(LwU32 lwPmc, LwU32 state) const
    {
        // Always enabled
        return lwPmc;
    }

    virtual void engineReset(GpuSubdevice* pSubdev) const
    {
        pSubdev->RegWr32(LW_PGSP_FALCON_ENGINE, REF_DEF(LW_PGSP_FALCON_ENGINE_RESET, _TRUE));
        pSubdev->RegWr32(LW_PGSP_FALCON_ENGINE, REF_DEF(LW_PGSP_FALCON_ENGINE_RESET, _FALSE));
    }

    virtual ~TuringGspInfo(void)
    {
    }
};

static TuringSecInfo sec2;
static TuringGspInfo gsp;

const FalconInfo *FalconInfo::getTuringFalconInfo(enum FALCON_INSTANCE instance)
{
    switch (instance) {
    case FalconInfo::FALCON_GSP:
        return &gsp;
    case FalconInfo::FALCON_SEC2:
        return &sec2;
    case FalconInfo::FALCON_PMU:
    case FalconInfo::FALCON_LWDEC:
        return FalconInfo::getPascalFalconInfo(instance);
    }

    return NULL;
}
