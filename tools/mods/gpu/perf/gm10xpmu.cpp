/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
*/

/*
 GM10x implementation of the PMU class
 */
#include "gpu/perf/pmusub.h"
#include "maxwell/gm108/dev_master.h"
#include "maxwell/gm108/dev_pwr_pri.h"
#include <vector>
#include "gpu/utility/scopereg.h"

//------------------------------------------------------------------------------
class GM10xPMU : public PMU
{
public:
    GM10xPMU(GpuSubdevice * pGpuSub) : PMU(pGpuSub)
    {
        m_Parent = pGpuSub;
    }

    virtual ~GM10xPMU() {}

    // APIs to reset, load ucode and execute ucode in PMU *without* RM loaded
    virtual RC ResetPmuWithoutRm();
    virtual RC LoadPmuDmem
    (
        UINT32 PmuPort, // DMEM access port
        UINT32 BlockNum, // Block index to be written to
        const  vector<UINT32> &PmuData,
        const  PMU::PmuUcodeInfo &DataSection
    );

    virtual RC LoadPmuImem
    (
        UINT32 PmuPort, // IMEM access port
        UINT32 BlockNum, // Block index to be written to
        const  vector<UINT32> &PmuCode,
        const  PMU::PmuUcodeInfo &CodeSection
    );

    virtual RC ExelwtePmuWithoutRm();

protected:

private:
    static bool PollPmuScrubbingDone(void * pArgs);
    static bool PollPmuHalted(void * pArgs);
};

//------------------------------------------------------------------------------
PMU * GM10xPMUFactory (GpuSubdevice * pGpuSub)
{
    switch (pGpuSub->DeviceId())
    {
        case Gpu::GM107:
        case Gpu::GM108:
        case Gpu::GM200:
        case Gpu::GM204:
        case Gpu::GM206:
            return new GM10xPMU(pGpuSub);
            break;
        default:
            return nullptr;
    }
}

/* static */ bool GM10xPMU::PollPmuScrubbingDone(void * pArgs)
{
    GpuSubdevice* pSubdev = (GpuSubdevice*)pArgs;
    UINT32 FalconDmaCtl = pSubdev->RegRd32(LW_PPWR_FALCON_DMACTL);
    UINT32 DmemScrub = DRF_VAL(_PPWR,
                               _FALCON_DMACTL,
                               _DMEM_SCRUBBING,
                               FalconDmaCtl);
    UINT32 ImemScrub = DRF_VAL(_PPWR,
                               _FALCON_DMACTL,
                               _IMEM_SCRUBBING,
                               FalconDmaCtl);

    if ((DmemScrub == LW_PPWR_FALCON_DMACTL_DMEM_SCRUBBING_DONE) &&
        (ImemScrub == LW_PPWR_FALCON_DMACTL_IMEM_SCRUBBING_DONE))
    {
        return true;
    }
    return false;
}

/* virtual */ RC GM10xPMU::ResetPmuWithoutRm()
{
    RC rc;
    const FLOAT64 PMU_RESET_TIMEOUT_MS = 500;

    // Reset the PMU
    m_Parent->DisablePwrBlock();
    m_Parent->EnablePwrBlock();

    UINT32 FalconCpuCtl = m_Parent->RegRd32(LW_PPWR_FALCON_CPUCTL);
    m_Parent->RegWr32(LW_PPWR_FALCON_CPUCTL,
              FLD_SET_DRF(_PPWR_FALCON, _CPUCTL, _HRESET, _TRUE, FalconCpuCtl));

    // Wait for the scrubbing to stop
    CHECK_RC(POLLWRAP_HW(&PollPmuScrubbingDone,
                         m_Parent,
                         PMU_RESET_TIMEOUT_MS));

    // Disable context DMA support in PMU, by setting REQUIRE_CTX to FALSE
    UINT32 FalconeDmaCtl = m_Parent->RegRd32(LW_PPWR_FALCON_DMACTL);
    m_Parent->RegWr32(LW_PPWR_FALCON_DMACTL,
       FLD_SET_DRF(_PPWR_FALCON, _DMACTL, _REQUIRE_CTX, _FALSE, FalconeDmaCtl));

    return rc;
}

/* virtual */ RC GM10xPMU::LoadPmuDmem
(
    UINT32 PmuPort, // DMEM access port
    UINT32 BlockNum, // Block index to be written to
    const  vector<UINT32> &PmuData,
    const  PMU::PmuUcodeInfo &DataSection
)
{
    RC rc;

    MASSERT(PmuData.size());

    // Ensure that both data and offset are 256 byte aligned
    // Check Offset + size must not be out of range for the ucode data
    if ((DataSection.OffsetInBytes & 0xFF) ||
        (DataSection.SizeInBytes & 0xFF) ||
        (PmuData.size() <
         (DataSection.OffsetInBytes + DataSection.SizeInBytes)/4))
    {
        Printf(Tee::PriError,
               "LoadPmuDmem: "
               "Invalid offset = %d,size = %d bytes.\n",
               DataSection.OffsetInBytes,
               DataSection.SizeInBytes);
        return RC::BAD_PARAMETER;
    }

    ScopedRegister RestoreContentAccessCtrl(m_Parent,
                                            LW_PPWR_FALCON_DMEMC(PmuPort));
    UINT32 DmemContentAccessCtrl = RestoreContentAccessCtrl;

    // Find the starting 32-bit data block in the ucode data blob
    UINT32 Offset = DataSection.OffsetInBytes/4;

    DmemContentAccessCtrl = FLD_SET_DRF_NUM(_PPWR_FALCON, _DMEMC, _OFFS, 0,
                                            DmemContentAccessCtrl);
    DmemContentAccessCtrl = FLD_SET_DRF_NUM(
                                           _PPWR_FALCON, _DMEMC, _BLK, BlockNum,
                                           DmemContentAccessCtrl);
    DmemContentAccessCtrl = FLD_SET_DRF(_PPWR_FALCON, _DMEMC, _AINCW, _TRUE,
                                        DmemContentAccessCtrl);

    m_Parent->RegWr32(LW_PPWR_FALCON_DMEMC(PmuPort), DmemContentAccessCtrl);

    for (UINT32 idx = 0; idx < (DataSection.SizeInBytes/4); idx++)
    {
        m_Parent->RegWr32(LW_PPWR_FALCON_DMEMD(PmuPort),
                FLD_SET_DRF_NUM(_PPWR_FALCON, _DMEMD, _DATA,
                                PmuData[Offset + idx], 0));
    }

    return rc;
}

/* virtual */ RC GM10xPMU::LoadPmuImem
(
    UINT32 PmuPort, // IMEM access port
    UINT32 BlockNum, // Block index to be written to
    const  vector<UINT32> &PmuCode,
    const  PMU::PmuUcodeInfo &CodeSection
)
{
    RC rc;

    MASSERT(PmuCode.size());

    // Ensure that both data and offset are 256 byte aligned
    // Check Offset + size must not be out of range for the ucode data
    if ((CodeSection.OffsetInBytes & 0xFF) ||
        (CodeSection.SizeInBytes & 0xFF) ||
        (PmuCode.size() <
         (CodeSection.OffsetInBytes + CodeSection.SizeInBytes)/4))
    {
        Printf(Tee::PriError,
               "LoadPmuImem: "
               "Invalid offset = %d,size = %d bytes.\n",
               CodeSection.OffsetInBytes,
               CodeSection.SizeInBytes);
        return RC::BAD_PARAMETER;
    }

    ScopedRegister RestoreContentAccessCtrl(m_Parent,
                                            LW_PPWR_FALCON_IMEMC(PmuPort));
    UINT32 ImemContentAccessCtrl = RestoreContentAccessCtrl;

    // Find the starting 32-bit data block in the ucode data blob
    UINT32 Offset = CodeSection.OffsetInBytes/4;

    ImemContentAccessCtrl = FLD_SET_DRF_NUM(_PPWR_FALCON, _IMEMC, _OFFS, 0,
                                            ImemContentAccessCtrl);
    ImemContentAccessCtrl = FLD_SET_DRF_NUM(
                                           _PPWR_FALCON, _IMEMC, _BLK, BlockNum,
                                           ImemContentAccessCtrl);
    ImemContentAccessCtrl = FLD_SET_DRF(_PPWR_FALCON, _IMEMC, _AINCW, _TRUE,
                                        ImemContentAccessCtrl);

    // Check if the this code section has to be marked "secure"
    if (CodeSection.bIsSelwre)
    {
        ImemContentAccessCtrl = FLD_SET_DRF_NUM(_PPWR_FALCON, _IMEMC,
                                                _SELWRE, 0x1,
                                                ImemContentAccessCtrl);
    }

    m_Parent->RegWr32(LW_PPWR_FALCON_IMEMC(PmuPort), ImemContentAccessCtrl);

    UINT32 LwrrTag = CodeSection.OffsetInBytes >> 8;
    for (UINT32 idx = 0; idx < (CodeSection.SizeInBytes/4); idx++)
    {
        // Setup the tag mapping to increment every 256 bytes
        if (!(idx % 64))
        {
            // End of a 256 byte block, setup the tag map
            m_Parent->RegWr32(LW_PPWR_FALCON_IMEMT(PmuPort),
                       FLD_SET_DRF_NUM(_PPWR_FALCON, _IMEMT, _TAG, LwrrTag, 0));
            LwrrTag++;
        }

        m_Parent->RegWr32(LW_PPWR_FALCON_IMEMD(PmuPort),
                          FLD_SET_DRF_NUM(_PPWR_FALCON, _IMEMD, _DATA,
                          PmuCode[Offset + idx], 0));
    }

    return rc;
}

/* static */ bool GM10xPMU::PollPmuHalted(void * pArgs)
{
    GpuSubdevice* pSubdev = (GpuSubdevice*)pArgs;
    UINT32 FalconCpuCtl = pSubdev->RegRd32(LW_PPWR_FALCON_CPUCTL);

    return (FLD_TEST_DRF(_PPWR, _FALCON_CPUCTL, _HALTED, _TRUE, FalconCpuCtl));
}

/* virtual */ RC GM10xPMU::ExelwtePmuWithoutRm()
{
    RC rc;
    const FLOAT64 PMU_HALT_TIMEOUT_MS = 500;

    // Set BOOTVEC to 0
    m_Parent->RegWr32(LW_PPWR_FALCON_BOOTVEC, 0);

    // Start the PMU
    m_Parent->RegWr32(LW_PPWR_FALCON_CPUCTL,
                      FLD_SET_DRF(_PPWR_FALCON, _CPUCTL, _STARTCPU, _TRUE, 0));

    // Check if the PMU has halted
    CHECK_RC(POLLWRAP_HW(&PollPmuHalted,
                         m_Parent,
                         PMU_HALT_TIMEOUT_MS));

    return rc;
}
