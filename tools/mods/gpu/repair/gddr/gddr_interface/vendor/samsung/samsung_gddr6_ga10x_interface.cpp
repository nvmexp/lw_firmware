/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "samsung_gddr6_ga10x_interface.h"

using namespace Memory;
using namespace Repair;

using RamVendorId   = FrameBuffer::RamVendorId;
using RamProtocol   = FrameBuffer::RamProtocol;

/******************************************************************************/
RC Gddr::SamsungGddr6GA10x::DoHardRowRepair
(
    Row row
) const
{
    RC rc;
    RegHal& regs = m_pSubdev->Regs();

    UINT32 hwFbpa        = row.hwFbio.Number();
    UINT32 subpIdx       = row.subpartition.Number();
    UINT32 pseudoChannel = row.pseudoChannel.Number();
    UINT32 bank          = row.bank.Number();
    UINT32 rowNum        = row.row;

    MASSERT(subpIdx <= 1);
    UINT32 subpMask = (2 - subpIdx) << 30; // For the MRS registers,
                                           // LW_PFB_FBPA_*_MRS<>, SUBP_MASK - [31:30]

    if (pseudoChannel == s_UnknownPseudoChannel)
    {
        Printf(Tee::PriError, "Unknown pseudo channel. Incomplete row repair information\n");
        return RC::SOFTWARE_ERROR;
    }

    if (!IsRowRepairable(row))
    {
        Printf(Tee::PriWarn, "No spares available for repair\n");
        return RC::REPAIR_INSUFFICIENT;
    }

    Settings *pSettings = GetSettings();
    MASSERT(pSettings);

    if (!pSettings->modifiers.printRegSeq)
    {
        if ((regs.IsSupported(MODS_PFB_FBPA_MEM_PRIV_LEVEL_MASK) &&
            !regs.Test32(MODS_PFB_FBPA_MEM_PRIV_LEVEL_MASK_WRITE_PROTECTION_ALL_LEVELS_ENABLED)) ||
            (regs.IsSupported(MODS_PFB_FBPA_MEM_DRAMID_PRIV_LEVEL_MASK) &&
            !regs.Test32(MODS_PFB_FBPA_MEM_DRAMID_PRIV_LEVEL_MASK_WRITE_PROTECTION_ALL_LEVELS_ENABLED)) ||
            (regs.IsSupported(MODS_PFB_FBPA_HISECTESTCMD_PRIV_LEVEL_MASK) &&
            !regs.Test32(MODS_PFB_FBPA_HISECTESTCMD_PRIV_LEVEL_MASK_WRITE_PROTECTION_ALL_LEVELS_ENABLED)))
        {
            Printf(Tee::PriError, "Missing write permission on registers\n");
            return RC::PRIV_LEVEL_VIOLATION;
        }
    }

    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_CFG0_ABI, 0); // GPU ABI OFF

    // ABI OFF in MR1 bit[10]: may refer to GDDR6/6X as per memory spec
    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_GENERIC_MRS1_ADR_GDDR6X_CABI, 1);

    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_REFCTRL_VALID, 0);  // Disable memory refresh
    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_CFG0_DRAM_ACPD, 0); // Disable ACPD

    // [9:8]/[1:0] = 00b to enable channel(16 bit) B/A inside SUBP
    // [31:30] for SUBP;
    UINT32 mrs15Val = (pseudoChannel == 1) ? 0x00f00003 : 0x00f00300;
    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_GENERIC_MRS15, mrs15Val);

    // TMRS entry code for BIOS without Samsung TMRS sequence
    // TODO - Figure out how to check the above
    // Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_0_GENERIC_MRS14, hwFbpa, subpMask | 0x88);  // SAFE1
    // Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_0_GENERIC_MRS14, hwFbpa, subpMask | 0x88);  // SAFE2
    // Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_0_GENERIC_MRS14, hwFbpa, subpMask | 0x88);  // SAFE3
    // Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_0_GENERIC_MRS14, hwFbpa, subpMask | 0xFF);  // SAFETEST

    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_0_GENERIC_MRS14, hwFbpa, subpMask | 0x82);  // CATSET(1)
    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_0_GENERIC_MRS14, hwFbpa, subpMask | 0x84);  // CATSET(2)
    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_0_GENERIC_MRS14, hwFbpa, subpMask | 0x81);  // GRPSET(0)
    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_0_GENERIC_MRS14, hwFbpa, subpMask | 0x88);  // LATCH
    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_0_GENERIC_MRS14, hwFbpa, subpMask | 0x88);  // LATCH_EXT
    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_0_GENERIC_MRS14, hwFbpa, subpMask | 0x880); // CATSET(A)
    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_0_GENERIC_MRS14, hwFbpa, subpMask | 0xa0);  // CATSET(5)
    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_0_GENERIC_MRS14, hwFbpa, subpMask | 0x82);  // GRPSET(1)
    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_0_GENERIC_MRS14, hwFbpa, subpMask | 0x88);  // LATCH
    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_0_GENERIC_MRS14, hwFbpa, subpMask | 0x88);  // LATCH_EXT

    // Toggle FBPA_CFG0_DRAM_ACK to clear FIFO. Needed for the last MR write
    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_CFG0_DRAM_ACK, 1);
    // MR13 = 1 (BA = 13, ADR = 1)
    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_0_GENERIC_MRS14, hwFbpa, subpMask | 0x00d00001);
    // Toggle FBPA_CFG0_DRAM_ACK to clear FIFO. Needed for the last MR write
    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_CFG0_DRAM_ACK, 0);

    UINT32 testCmd = 0;
    UINT32 testCmdExt = 0;
    UINT32 testCmdExt1 = 0;
    if (pseudoChannel == 1)
    {
        // FBPA_TESTCMD_EXT:
        // [0]=CA9_R=R13; LAM=1 for LDFF/ACT/MRS.
        regs.SetField(&testCmdExt, MODS_PFB_FBPA_0_TESTCMD_EXT_ADR_DRAM1, hwFbpa, (rowNum & 0xFF));
        regs.SetField(&testCmdExt, MODS_PFB_FBPA_0_TESTCMD_EXT_LAM_CMD,  hwFbpa, 1);

        // FBPA_TESTCMD:
        testCmd |= (1 << 30);
        testCmd |= (bank << 8);
        testCmd |= (1 << 6);
        testCmd |= (((rowNum >> 14) & 0x1) << 5);
        testCmd |= (((rowNum >> 12) & 0x3) << 3);
        testCmd |= (((rowNum >> 8) & 0x3) << 1);
    }
    else
    {
        // FBPA_TESTCMD_EXT:
        // [0]=CA9_R=R13. LAM=1 for LDFF/ACT/MRS
        regs.SetField(&testCmdExt, MODS_PFB_FBPA_0_TESTCMD_EXT_LAM_CMD,  hwFbpa, 1);

        // FBPA_TESTCMD:
        testCmd |= (1 << 30);
        testCmd |= ((rowNum & 0xFF) << 12);
        testCmd |= (bank << 8);
        testCmd |= (1 << 6);
        testCmd |= (((rowNum >> 14) & 0x1) << 5);
        testCmd |= (((rowNum >> 12) & 0x3) << 3);
        testCmd |= (((rowNum >> 8) & 0x3) << 1);
    }
    // FBPA_TESTCMD_EXT1:
    // [28]=CA7_F=R11; [27]=CA6_F=R10
    regs.SetField(&testCmdExt1, MODS_PFB_FBPA_0_TESTCMD_EXT_1_CSB0, hwFbpa, (rowNum >> 10) & 0x1);
    regs.SetField(&testCmdExt1, MODS_PFB_FBPA_0_TESTCMD_EXT_1_CSB1, hwFbpa, (rowNum >> 11) & 0x1);

    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_0_TESTCMD_EXT_1, hwFbpa, testCmdExt1);
    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_0_TESTCMD_EXT, hwFbpa, testCmdExt);
    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_0_TESTCMD, hwFbpa, testCmd);

    SleepMs(2000);

    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_0_TESTCMD_EXT_1, hwFbpa, 0);
    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_0_TESTCMD_EXT, hwFbpa, 1);

    testCmd = 0;
    regs.SetField(&testCmd, MODS_PFB_FBPA_0_TESTCMD_GO, hwFbpa, 1);
    regs.SetField(&testCmd, MODS_PFB_FBPA_0_TESTCMD_RES, hwFbpa, 1);
    regs.SetField(&testCmd, MODS_PFB_FBPA_0_TESTCMD_CS0, hwFbpa, 1);
    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_0_TESTCMD, hwFbpa, testCmd);

    // Toggle CKE
    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_PIN_CKE, 0);
    SleepUs(1);
    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_PIN_CKE, 1);

    SleepUs(10);

    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_0_GENERIC_MRS14, hwFbpa, subpMask | 0x00d00000);
    SleepUs(50);

    // Restore state
    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_0_GENERIC_MRS14, hwFbpa, 0x00e00000); // Restore MRS14
    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_GENERIC_MRS15, 0x00f00000);           // Restore MRS15
    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_REFCTRL_VALID, 1);                    // Enable memory refresh
    Repair::Write32(*pSettings, regs, MODS_PFB_FBPA_CFG0_DRAM_ACPD, 1);                   // Enable ACPD

    return rc;
}
