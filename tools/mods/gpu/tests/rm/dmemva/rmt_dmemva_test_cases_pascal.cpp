#include "rmt_dmemva_test_cases.h"

#include "lwmisc.h"
#include "core/include/lwrm.h"
#include "core/include/gpu.h"
#include "gpu/include/gpusbdev.h"

#include "pascal/gp102/dev_master.h"
#include "pascal/gp102/dev_falcon_v4.h"
#include "pascal/gp102/dev_sec_pri.h"
#include "pascal/gp102/dev_pwr_pri.h"
#include "pascal/gp102/dev_lwdec_pri.h"

#include "gpu/tests/rm/dmemva/ucode/g_dmvauc_sec2_gp10x.h"
#include "gpu/tests/rm/dmemva/ucode/g_dmvauc_pmu_gp10x.h"
#include "gpu/tests/rm/dmemva/ucode/g_dmvauc_lwdec_gp10x.h"
#include "gpu/tests/rm/dmemva/ucode/g_dmvauc_sec2_gp10x_gp102_sig.h"
#include "gpu/tests/rm/dmemva/ucode/g_dmvauc_pmu_gp10x_gp102_sig.h"
#include "gpu/tests/rm/dmemva/ucode/g_dmvauc_lwdec_gp10x_gp102_sig.h"

class PascalSecInfo : public FalconInfo
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
        return dmva_ucode_data_sec2_gp10x;
    }

    virtual LwU32 *getUcodeHeader(void) const
    {
        return dmva_ucode_header_sec2_gp10x;
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

    virtual ~PascalSecInfo(void)
    {
    }
};

class PascalPmuInfo : public FalconInfo
{
   public:

    virtual FALCON_INSTANCE getEngineId(void) const
    {
        return FALCON_PMU;
    }

    virtual const char *getFalconName(void) const
    {
        return "PMU";
    }

    virtual LwU32 getEngineBase(void) const
    {
        return LW_FALCON_PWR_BASE;
    }

    virtual LwU32 *getUcodeData(void) const
    {
        return dmva_ucode_data_pmu_gp10x;
    }

    virtual LwU32 *getUcodeHeader(void) const
    {
        return dmva_ucode_header_pmu_gp10x;
    }

    virtual LwU32 getPatchLocation(void) const
    {
        return dmva_pmu_sig_patch_location[0];
    }

    virtual LwU32 getPatchSignature(void) const
    {
        return dmva_pmu_sig_patch_signature[0];
    }

    virtual LwU32 *getSignature(bool debug = true) const
    {
        if (debug)
            return dmva_pmu_sig_dbg;
        else
            return dmva_pmu_sig_prod;
    }

    virtual LwU32 setPMC(LwU32 lwPmc, LwU32 state) const
    {
        return FLD_SET_DRF_NUM(_PMC, _ENABLE, _PWR, state, lwPmc);
    }

    virtual void engineReset(GpuSubdevice* pSubdev) const
    {
        pSubdev->RegWr32(LW_PPWR_FALCON_ENGINE, REF_DEF(LW_PPWR_FALCON_ENGINE_RESET, _TRUE));
        pSubdev->RegWr32(LW_PPWR_FALCON_ENGINE, REF_DEF(LW_PPWR_FALCON_ENGINE_RESET, _FALSE));
    }

    virtual ~PascalPmuInfo(void)
    {
    }
};

class PascalLwdecInfo : public FalconInfo
{
   public:
    virtual FALCON_INSTANCE getEngineId(void) const
    {
        return FALCON_LWDEC;
    }

    virtual const char *getFalconName(void) const
    {
        return "LWDEC";
    }

    virtual LwU32 getEngineBase(void) const
    {
        return LW_FALCON_LWDEC_BASE;
    }

    virtual LwU32 *getUcodeData(void) const
    {
        return dmva_ucode_data_lwdec_gp10x;
    }

    virtual LwU32 *getUcodeHeader(void) const
    {
        return dmva_ucode_header_lwdec_gp10x;
    }

    virtual LwU32 getPatchLocation(void) const
    {
        return dmva_lwdec_sig_patch_location[0];
    }

    virtual LwU32 getPatchSignature(void) const
    {
        return dmva_lwdec_sig_patch_signature[0];
    }

    virtual LwU32 *getSignature(bool debug = true) const
    {
        if (debug)
            return dmva_lwdec_sig_dbg;
        else
            return dmva_lwdec_sig_prod;
    }

    virtual LwU32 setPMC(LwU32 lwPmc, LwU32 state) const
    {
        return FLD_SET_DRF_NUM(_PMC, _ENABLE, _LWDEC, state, lwPmc);
    }

    virtual void engineReset(GpuSubdevice* pSubdev) const
    {
        // Do nothing
    }

    virtual ~PascalLwdecInfo(void)
    {
    }
};

static PascalSecInfo sec2;
static PascalPmuInfo pmu;
static PascalLwdecInfo lwdec;

const FalconInfo *FalconInfo::getPascalFalconInfo(enum FALCON_INSTANCE instance)
{
    switch (instance) {
    case FalconInfo::FALCON_SEC2:
        return &sec2;
    case FalconInfo::FALCON_PMU:
        return &pmu;
    case FalconInfo::FALCON_LWDEC:
        return &lwdec;
    default:
        return NULL;
    }
}
