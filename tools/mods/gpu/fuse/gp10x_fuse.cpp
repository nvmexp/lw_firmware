/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gp10x_fuse.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/utility/scopereg.h"
#include "core/include/utility.h"
#include "core/include/crypto.h"
#include "fusesrc.h"
#include "core/include/fileholder.h"
#include "core/include/platform.h"
#include "mods_reg_hal.h"
#include "pascal/gp100/dev_fuse.h"
#include "pascal/gp100/dev_pwr_pri.h"
#include "pascal/gp100/dev_bus.h"
#include "pascal/gp100/dev_trim.h"
#include "pascal/gp100/dev_gc6_island.h"

//-----------------------------------------------------------------------------
GP10xFuse::GP10xFuse(Device* pSubdev): GM20xFuse(pSubdev)
{
    const FuseUtil::FuseDefMap* pFuseDefMap = nullptr;
    const FuseUtil::SkuList* pSkuList = nullptr;
    const FuseUtil::MiscInfo* pMiscInfo = nullptr;
    const FuseDataSet* pFuseDataset = nullptr;

    RC rc = ParseXmlInt(&pFuseDefMap, &pSkuList, &pMiscInfo, &pFuseDataset);

    if (rc == OK)
    {
        m_FuseSpec.NumOfFuseReg  = pMiscInfo->MacroInfo.FuseRows;

        MASSERT(pMiscInfo->MacroInfo.FuseRecordStart * pMiscInfo->MacroInfo.FuseCols
            == pMiscInfo->FuselessStart);
        m_FuseSpec.FuselessStart = pMiscInfo->FuselessStart;    //ramRepairFuseBlockBegin

        MASSERT(m_FuseSpec.NumOfFuseReg * pMiscInfo->MacroInfo.FuseCols - 1
            == pMiscInfo->FuselessEnd);
        m_FuseSpec.FuselessEnd   = pMiscInfo->FuselessEnd - 32;    //ramRepairFuseBlockEnd - 32
    }

    m_FuseSpec.NumKFuseWords = 192;

    m_FuseReadBinary = "fuseread_gp10x.bin";

    GpuSubdevice *pGpuSubdev = (GpuSubdevice*)GetDevice();
    switch (pGpuSubdev->DeviceId())
    {
        case Gpu::GP100:
            m_GPUSuffix = "GP100";
            // GP100 uses older falcon keys
            m_FuseReadBinary = "fuseread_gm206.bin";
            break;

        case Gpu::GP102:
            m_GPUSuffix = "GP102";
            break;

        case Gpu::GP104:
            m_GPUSuffix = "GP104";
            break;

        case Gpu::GP106:
            m_GPUSuffix = "GP106";
            break;

        case Gpu::GP107:
            m_GPUSuffix = "GP107";
            break;

        case Gpu::GP108:
            m_GPUSuffix = "GP108";
            break;

        default:
            m_GPUSuffix = "UNKNOWN";
            break;
    }

    m_SetFieldSpaceName[IFF_SPACE(PTRIM_8)] = "LW_PTRIM_8";
    m_SetFieldSpaceAddr[IFF_SPACE(PTRIM_8)] = DRF_BASE(LW_PTRIM) + 0x8000;

    // LW_PGC6 isn't 0x1000 aligned, thus HW masks out the last 12 bits.
    m_SetFieldSpaceName[IFF_SPACE(PGC6_0)] = "LW_PGC6";
    m_SetFieldSpaceAddr[IFF_SPACE(PGC6_0)] = DRF_BASE(LW_PGC6) & ~0xFFF;
}

//-----------------------------------------------------------------------------
RC GP10xFuse::LatchFuseToOptReg(FLOAT64 TimeoutMs)
{
    GpuSubdevice *pSubdev = (GpuSubdevice*)GetDevice();

    // Make sure sw fusing isn't enabled
    pSubdev->RegWr32(LW_FUSE_EN_SW_OVERRIDE, 0);

    // Reset control registers
    ScopedRegister FuseCtrl(pSubdev, LW_FUSE_FUSECTRL);
    pSubdev->RegWr32(LW_FUSE_FUSECTRL, FLD_SET_DRF(_FUSE,
                         _FUSECTRL, _CMD, _SENSE_CTRL, FuseCtrl));

    UINT32 Pri2Intfc = 0;
    Pri2Intfc = FLD_SET_DRF_NUM(_FUSE, _PRIV2INTFC, _START_DATA, 1, Pri2Intfc);
    pSubdev->RegWr32(LW_FUSE_PRIV2INTFC, Pri2Intfc);
    Platform::Delay(1);
    pSubdev->RegWr32(LW_FUSE_PRIV2INTFC, 0);   // reset _START and _RAMREPAIR
    Platform::Delay(4);
    return POLLWRAP_HW(GM20xFuse::PollFuseSenseDone, pSubdev, TimeoutMs);
}

//-----------------------------------------------------------------------------
/* virtual */ bool GP10xFuse::IsFusePrivSelwrityEnabled()
{
    // Look in the OPT registers if fusing is enabled
    // We need access to both FUSECTRL and FUSEWDATA to fuse,
    // so locking either one locks fusing
    GpuSubdevice *pSubdev = (GpuSubdevice*)GetDevice();
    return FLD_TEST_DRF(_FUSE, _OPT_PRIV_SEC_EN, _DATA, _YES,
                        pSubdev->RegRd32(LW_FUSE_OPT_PRIV_SEC_EN)) &&
          (FLD_TEST_DRF(_FUSE, _OPT_SELWRE_FUSE_WDATA_WR_SELWRE, _DATA, _ENABLE,
                        pSubdev->RegRd32(LW_FUSE_OPT_SELWRE_FUSE_WDATA_WR_SELWRE)) ||
           FLD_TEST_DRF(_FUSE, _OPT_SELWRE_FUSE_CTRL_WR_SELWRE, _DATA, _ENABLE,
                        pSubdev->RegRd32(LW_FUSE_OPT_SELWRE_FUSE_CTRL_WR_SELWRE)));

}

//-----------------------------------------------------------------------------
RC GP10xFuse::GetSelwreUcode
(
    bool isPMUBinaryEncrypted,
    vector<UINT32>* pBinary
)
{
    RC rc;

    string pmuBinFilename = "fusebinary_" + m_GPUSuffix + ".bin";
    CHECK_RC(FalconUCode::ReadBinary(pmuBinFilename, pBinary));

    if (isPMUBinaryEncrypted)
    {
        Crypto::XTSAESKey xtsAesKey;
        memset(&xtsAesKey, 0 , sizeof(xtsAesKey));
        memcpy(&xtsAesKey.keys[0], m_L2Key.c_str(), FUSE_KEY_LENGTH);

        vector<UINT32> decryptedBin(pBinary->size());
        CHECK_RC(Crypto::XtsAesDecSect(
                 xtsAesKey,
                 0,
                 static_cast<UINT32>(pBinary->size() * sizeof(UINT32) / sizeof(UINT08)),
                 reinterpret_cast<const UINT08 *>(&pBinary->front()),
                 reinterpret_cast<UINT08 *>(&decryptedBin.front())));

        *pBinary = decryptedBin;
    }

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC GP10xFuse::EnableSelwreFuseblow(bool isPMUBinaryEncrypted)
{
    RC rc;

    GpuSubdevice *pSubdev = (GpuSubdevice*)GetDevice();

    vector<UINT32> reworkBinary;
    GetSelwreUcode(isPMUBinaryEncrypted, &reworkBinary);

    GM20xFalcon falcon(pSubdev, GM20xFalcon::FalconType::PMU);
    CHECK_RC(falcon.Reset());
    CHECK_RC(falcon.LoadBinary(reworkBinary, FalconUCode::UCodeDescType::LEGACY_V1));
    CHECK_RC(falcon.Start(true));

    // Check for secure PMU ucode status (mailbox register)
    UINT32 PmuStatus;
    CHECK_RC(falcon.ReadMailbox(0, &PmuStatus));

    switch (PmuStatus & 0xffff)
    {
        case SELWRE_PMU_STATUS_SUCCESS:
            break;

        case SELWRE_PMU_STATUS_UCODE_VERSION_MISMATCH:
            Printf(Tee::PriHigh,
                   "Ucode version mismatch !\n");
            return RC::CMD_STATUS_ERROR;

        case SELWRE_PMU_STATUS_FAILURE:
            Printf(Tee::PriHigh,
                   "PMU ucode failed to lower privilege level\n");
            return RC::CMD_STATUS_ERROR;

        case SELWRE_PMU_STATUS_UNSUPPORTED_GPU:
            Printf(Tee::PriError,
                   "Unsupported GPU ID, Found 0x%08x\n", PmuStatus >> 16);
            return RC::UNSUPPORTED_FUNCTION;

        default:
            Printf(Tee::PriHigh,
                   "Unknown PMU status code = 0x%08x \n", PmuStatus);
            return RC::UNSUPPORTED_FUNCTION;
    }

    // One last check to confirm if the security privilege levels have been
    // lowered to proceed with secure fuseblow
    CHECK_RC(CheckFuseblowEnabled());

    return rc;
}

//-----------------------------------------------------------------------------
RC GP10xFuse::CheckFuseblowEnabled()
{
    GpuSubdevice *pSubdev = (GpuSubdevice*)GetDevice();
    if (!pSubdev->Regs().Test32(MODS_FUSE_WDATA_PRIV_LEVEL_MASK_WRITE_PROTECTION_ALL_LEVELS_ENABLED) ||
        !pSubdev->Regs().Test32(MODS_FUSE_CTRL_PRIV_LEVEL_MASK_WRITE_PROTECTION_ALL_LEVELS_ENABLED))
    {
        Printf(Tee::PriError, "Secure PMU could not be loaded !\n");
        return RC::PMU_DEVICE_FAIL;
    }
    return OK;
}

//-----------------------------------------------------------------------------
UINT32 GP10xFuse::CreateRIRRecord(UINT32 bit, bool set1, bool disable)
{
    // Max of 13 address bits
    MASSERT((bit & 0xffffe000) == 0);

    // Bit assignments for a 16 bit RIR record
    // From TSMC Electrical Fuse Datasheet (TEF16FGL192X32HD18_PHENRM_C140218)
    // 15:      Disable
    // 14-10:   Addr[4-0]
    // 9-8:     Addr[6-5]
    // 7-2:     Addr[12-7]
    // 1:       Data
    // 0:       Enable
    UINT16 newRecord = 1;
    newRecord |= (set1 ? 1 : 0) << 1;
    newRecord |= ((bit & 0x1f80) >> 7) << 2;
    newRecord |= ((bit & 0x0060) >> 5) << 8;
    newRecord |= ((bit & 0x001f) >> 0) << 10;
    newRecord |= (disable ? 1 : 0) << 15;

    return newRecord;
}
