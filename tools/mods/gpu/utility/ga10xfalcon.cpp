/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "ga10xfalcon.h"

//-----------------------------------------------------------------------------
GA10xFalcon::GA10xFalcon(GpuSubdevice* pSubdev, FalconType falconType)
    : GM20xFalcon(pSubdev, falconType)
    , m_pSubdev(pSubdev)
{
    MASSERT(m_pSubdev);
}

RC GA10xFalcon::Initialize()
{
    RC rc;
    if (!IsInitialized())
    {
        CHECK_RC(GM20xFalcon::Initialize());
        // Set core to falcon
        // It is possible that the core is already locked to RISCV though, which requires
        // a engine reset and this will call will fail
        CHECK_RC(SetFalconCore());
    }
    return RC::OK;
}

RC GA10xFalcon::GetUCodeVersion(UINT32 ucodeId, UINT32 *pVersion)
{
    RC rc;
    MASSERT(pVersion);

    CHECK_RC(Initialize());
    // Ucode IDs start from 1
    CHECK_RC(Falcon2RegRd(MODS_PFALCON2_FALCON_UCODE_VERSION, ucodeId - 1, pVersion));

    return rc;
}

RC GA10xFalcon::Reset()
{
    RC rc;
    RegHal &regs = m_pSubdev->Regs();

    CHECK_RC(Initialize());

    // Following sequence in http://lwbugs/2892652

    // Always try the Falcon reset path first because it is easier
    UINT32 bcrCtrl;
    CHECK_RC(Falcon2RegRd(MODS_PFALCON2_RISCV_BCR_CTRL, &bcrCtrl));
    if (regs.TestField(bcrCtrl, MODS_PFALCON2_RISCV_BCR_CTRL_VALID_FALSE))
    {
        CHECK_RC(SetFalconCore());
    }

    CHECK_RC(Falcon2RegRd(MODS_PFALCON2_RISCV_BCR_CTRL, &bcrCtrl));
    if (regs.TestField(bcrCtrl, MODS_PFALCON2_RISCV_BCR_CTRL_CORE_SELECT_FALCON))
    {
        // Reset through falcon
        CHECK_RC(WaitForKfuseSfkLoad(1000)); // Timeout is arbitrarily chosen
        CHECK_RC(ApplyEngineReset());
        CHECK_RC(SetFalconCore());           // Set core back to falcon after reset
        CHECK_RC(WaitForKfuseSfkLoad(1000)); // Timeout is arbitrarily chosen

        PollFalconArgs pollArgs;
        pollArgs.pSubdev    = m_pSubdev;
        pollArgs.engineBase = GetEngineBase2();

        // Wait for the scrubbing to stop
        CHECK_RC(POLLWRAP_HW(&PollScrubbingDone, &pollArgs, AdjustTimeout(1000)));
    }
    else
    {
        // Reset through RISCV
        UINT32 brRetCode;
        CHECK_RC(Falcon2RegRd(MODS_PFALCON2_RISCV_BR_RETCODE, &brRetCode));
        if (regs.TestField(brRetCode, MODS_PFALCON2_RISCV_BR_RETCODE_RESULT_INIT))
        {
            // Someone just started riscv / core has never started / back to back reset
            // Delay for 150 us
            Utility::Delay(static_cast<UINT32>(AdjustTimeout(150)));
            CHECK_RC(ApplyEngineReset());
        }
        else if (regs.TestField(brRetCode, MODS_PFALCON2_RISCV_BR_RETCODE_RESULT_PASS))
        {
            CHECK_RC(ApplyEngineReset());
        }
        else if (regs.TestField(brRetCode, MODS_PFALCON2_RISCV_BR_RETCODE_RESULT_FAIL))
        {
            // Keyglob transmission might still be in progress, not safe to reset
            // Engine unusable at this point, skip reset
            Printf(Tee::PriError, "BR failed, not safe to reset\n");
            return RC::SOFTWARE_ERROR;
        }
        else if (regs.TestField(brRetCode, MODS_PFALCON2_RISCV_BR_RETCODE_RESULT_RUNNING))
        {
            // BR is running, unsafe reset - do not reset;
            Printf(Tee::PriError, "BR running, not safe to reset\n");
            return RC::SOFTWARE_ERROR;
        }

        // Set core back to falcon
        CHECK_RC(SetFalconCore());
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC GA10xFalcon::LoadProgram
(
    const vector<UINT32>& imemBinary,
    const vector<UINT32>& dmemBinary,
    const FalconUCode::UCodeInfo &ucodeInfo
)
{
    RC rc;
    RegHal &regs = m_pSubdev->Regs();

    // Get IMEM/DMEM size
    UINT32 val;
    CHECK_RC(FalconRegRd(MODS_PFALCON_FALCON_HWCFG, &val));
    // IMEM in units of 256 bytes.
    UINT32 falconIMEMSize = regs.GetField(val, MODS_PFALCON_FALCON_HWCFG_IMEM_SIZE);
    // DMEM in units of 256 bytes.
    UINT32 falconDMEMSize = regs.GetField(val, MODS_PFALCON_FALCON_HWCFG_DMEM_SIZE);
    falconIMEMSize *= 256;
    falconDMEMSize *= 256;

    Tee::Priority pri = GetVerbosePriority();
    MASSERT(falconIMEMSize / 4 >= imemBinary.size());
    MASSERT(falconDMEMSize / 4 >= dmemBinary.size());
    Printf(pri, "Max MEM Size - I: 0x%x, D: 0x%x\n", falconIMEMSize, falconDMEMSize);

    // See https://confluence.lwpu.com/display/GFWGA10X/Ampere-843%3A+Falcon+Boot+from+HS

    Printf(pri, "Loading IMEM/DMEM\n");
    CHECK_RC(SetFalconCore());
    CHECK_RC(TriggerAndPollScrub());
    CHECK_RC(WriteIMem(imemBinary, ucodeInfo.imemBase, ucodeInfo.secStart, ucodeInfo.secEnd));
    CHECK_RC(WriteDMem(dmemBinary, ucodeInfo.dmemBase));

    Printf(pri, "Loading metadata\n");
    CHECK_RC(InitSecSpr(ucodeInfo.bIsEncrypted));
    CHECK_RC(Falcon2RegWr(MODS_PFALCON2_FALCON_MOD_SEL, 0x1));
    CHECK_RC(Falcon2RegWr(MODS_PFALCON2_FALCON_BROM_ENGIDMASK, ucodeInfo.engIdMask));
    CHECK_RC(Falcon2RegWr(MODS_PFALCON2_FALCON_BROM_LWRR_UCODE_ID, ucodeInfo.ucodeId));
    CHECK_RC(Falcon2RegWr(MODS_PFALCON2_FALCON_BROM_PARAADDR, 0, ucodeInfo.pkcDataOffsetBytes));

    return rc;
}

RC GA10xFalcon::InitSecSpr(bool bIsEncrypted)
{
    RC rc;
    RegHal &regs = m_pSubdev->Regs();

    // See ICD sequence for loader programming from
    // https://confluence.lwpu.com/display/GFWGA10X/Ampere-843%3A+Falcon+Boot+from+HS
    UINT32 icdPlm = 0;
    CHECK_RC(FalconRegRd(MODS_PFALCON_FALCON_ICD_PRIV_LEVEL_MASK, &icdPlm));
    // Sequence is needed only on unfused parts (PLM controlled by FALCON_UCODE_VERSION(15)[0:0])
    if (regs.TestField(icdPlm, MODS_PFALCON_FALCON_ICD_PRIV_LEVEL_MASK_WRITE_PROTECTION_ALL_LEVELS_ENABLED))
    {
        // LW_SPR_SEC_SELWRE_BLK_BASE       15:0
        // LW_SPR_SEC_SELWRE                16:16
        // LW_SPR_SEC_ENCRYPTED             17:17
        // LW_SPR_SEC_SIGNATURE_VALID       18:18
        // LW_SPR_SEC_DISABLE_EXCEPTIONS    19:19
        // LW_SPR_SEC_SELWRE_BLK_CNT        31:24
        UINT32 secSprVal = 0;
        secSprVal |= (bIsEncrypted ? 1 << 17 : 0);
        secSprVal |= (0xFF << 24);

        UINT32 icdCmd = 0;
        CHECK_RC(FalconRegRd(MODS_PFALCON_FALCON_ICD_CMD, &icdCmd));
        regs.SetField(&icdCmd, MODS_PFALCON_FALCON_ICD_CMD_OPC_STOP);
        CHECK_RC(FalconRegWr(MODS_PFALCON_FALCON_ICD_CMD, icdCmd));

        CHECK_RC(FalconRegWr(MODS_PFALCON_FALCON_ICD_WDATA, secSprVal));

        CHECK_RC(FalconRegRd(MODS_PFALCON_FALCON_ICD_CMD, &icdCmd));
        regs.SetField(&icdCmd, MODS_PFALCON_FALCON_ICD_CMD_OPC_WREG);
        regs.SetField(&icdCmd, MODS_PFALCON_FALCON_ICD_CMD_IDX_SEC);
        CHECK_RC(FalconRegWr(MODS_PFALCON_FALCON_ICD_CMD, icdCmd));

        CHECK_RC(FalconRegRd(MODS_PFALCON_FALCON_ICD_CMD, &icdCmd));
        regs.SetField(&icdCmd, MODS_PFALCON_FALCON_ICD_CMD_OPC_RUN);
        CHECK_RC(FalconRegWr(MODS_PFALCON_FALCON_ICD_CMD, icdCmd));
    }
    else
    {
        Printf(GetVerbosePriority(), "Not programming ICD sequence\n");
    }

    return rc;
}

RC GA10xFalcon::SetFalconCore()
{
    RC rc;
    RegHal &regs = m_pSubdev->Regs();

    UINT32 bcrCtrl;
    CHECK_RC(Falcon2RegRd(MODS_PFALCON2_RISCV_BCR_CTRL, &bcrCtrl));

    if (regs.TestField(bcrCtrl, MODS_PFALCON2_RISCV_BCR_CTRL_CORE_SELECT_FALCON))
    {
        // Core already set to falcon
        return RC::OK;
    }

    if (regs.TestField(bcrCtrl, MODS_PFALCON2_RISCV_BCR_CTRL_VALID_TRUE))
    {
        Printf(Tee::PriError, "RISCV_BCR_CTRL is locked to RISCV core, needs reset\n");
        return RC::SOFTWARE_ERROR;
    }

    // Set core as falcon
    CHECK_RC(Falcon2RegRd(MODS_PFALCON2_RISCV_BCR_CTRL, &bcrCtrl));
    regs.SetField(&bcrCtrl, MODS_PFALCON2_RISCV_BCR_CTRL_CORE_SELECT_FALCON);
    CHECK_RC(Falcon2RegWr(MODS_PFALCON2_RISCV_BCR_CTRL, bcrCtrl));

    PollFalconArgs pollArgs;
    pollArgs.pSubdev    = m_pSubdev;
    pollArgs.engineBase = GetEngineBase2();

    // Poll till valid
    CHECK_RC(POLLWRAP_HW(&PollBcrCtrlValid, &pollArgs, AdjustTimeout(3000)));

    return rc;
}

RC GA10xFalcon::ResetPmu()
{
    // Disable PMU module
    m_pSubdev->DisablePwrBlock();
    // Wait 5ms for the reset
    Utility::Delay(static_cast<UINT32>(AdjustTimeout(5000)));
    // Enable PMU module, and PTimer
    m_pSubdev->EnablePwrBlock();

    return RC::OK;
}

RC GA10xFalcon::ResetSec()
{
    // Disable Sec module
    m_pSubdev->DisableSecBlock();
    // Wait 5ms for the reset
    Utility::Delay(static_cast<UINT32>(AdjustTimeout(5000)));
    // Enable SEC module
    m_pSubdev->EnableSecBlock();

    return RC::OK;
}

bool GA10xFalcon::PollBcrCtrlValid(void *pArgs)
{
    PollFalconArgs *pPollArgs  = (PollFalconArgs *)(pArgs);
    GpuSubdevice   *pSubdev   = pPollArgs->pSubdev;
    UINT32         baseAddr   = pPollArgs->engineBase;

    RegHal &regs = pSubdev->Regs();
    UINT32 bcrCtrl;
    bool bIsValid = false;
    if (FalconRegRdImpl(pSubdev, baseAddr, MODS_PFALCON2_RISCV_BCR_CTRL, &bcrCtrl) == RC::OK)
    {
        bIsValid = regs.TestField(bcrCtrl, MODS_PFALCON2_RISCV_BCR_CTRL_VALID_TRUE);
    }
    return bIsValid;
}
