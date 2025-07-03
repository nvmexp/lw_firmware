/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
// Mcp class code and device ids.
//------------------------------------------------------------------------------
// 45678901234567890123456789012345678901234567890123456789012345678901234567890

#ifndef INCLUDED_MCP_H
#define INCLUDED_MCP_H

//******************************************************************************
// Platform chipset
//******************************************************************************
// South Bridge /one chip solution
#define LW_MCP1  0x00000010
#define LW_MCP2  0x00000020
#define LW_CK8   0x00000030
#define LW_MCP2S 0x00000040
#define LW_CK8S  0x00000050
#define LW_MCP04 0x00000060
#define LW_CK804 0x00000070
#define LW_CK804_PRO 0x00000072
#define LW_CK804_SLV 0x00000071
#define LW_MCP51   0x00000080
#define LW_MCP51_G 0x00000081
#define LW_MCP51_M 0x00000082
#define LW_MCP55  0x000000A0
#define LW_MCP61  0x000000B0
#define LW_MCP65  0x000000C0
#define LW_MCP73  0x000000D0
#define LW_MCP77  0x000000E0
#define LW_MCP79  0x000000F0

// North Bridge
#define LW_C11   0x00100000
#define LW_C12   0x00200000
#define LW_C17   0x00300000
#define LW_C18   0x00400000
#define LW_C19   0x00500000
#define LW_C51   0x00600000
#define LW_C51_PV  0x00600000
#define LW_C51_G   0x00610000
#define LW_C51_PVG 0x00620000
#define LW_C51_MV  0x00630000
#define LW_C51_D   0x00640000
#define LW_C55   0x00700000
#define LW_C73   0x00800000

#define LW_MCP_ILWALID 0xffffffff

//******************************************************************************
// Standard Pci Cfg space
//******************************************************************************
#define PCI_VENDOR_ID_OFFSET           0x00  // 16 bit
#define PCI_DEVICE_ID_OFFSET           0x02  // 16 bit
#define PCI_COMMAND_OFFSET             0x04  // 16 bit, cfg1
#define PCI_CONTROL_STATUS_OFFSET      0x06
#define PCI_CLASS_CODE_OFFSET          0x08  // cfg 2
#define PCI_BAR_OFFSET(i)              (0x10+4*(i)) // i = 0 - 5. 6,7 reserved
#define PCI_BAR_0_OFFSET               0x10  // cfg 4
#define PCI_BAR_1_OFFSET               0x14  // cfg 5
#define PCI_BAR_2_OFFSET               0x18  // cfg 6
#define PCI_BAR_3_OFFSET               0x1c  // cfg 7
#define PCI_BAR_4_OFFSET               0x20  // cfg 8
#define PCI_BAR_5_OFFSET               0x24  // cfg 9
#define PCI_CAP_PTR_OFFSET             0x34  // cfg 13
#define PCI_INTR_LINE_OFFSET           0x3c  // cfg 15
#define PCI_CFG_OFFSET(i)             (4*i) // cfg i
#define PCI_CFG1_DISABLE_INTX 0x00000400 // bit10
#define PCI_CFG1_CAPLIST_EN   0x00100000 // bit 20
#define PCI_END_STD_CFG                0xff

// MSI data stucture offsets based on Msi base_offset
#define MSI_CTRL_OFFSET     0
#define MSI_CTRL_MULT_MSG_EN   22,20
#define MSI_CTRL_MULT_MSG_CAP  19,17
#define MSI_CTRL_EN         (1<<16)

#define MSI_ADDR_LO_OFFSET  4
#define MSI_ADDR_HI_OFFSET  8
#define MSI_DATA_32_OFFSET  8
#define MSI_DATA_64_OFFSET  12

#define MSIX_CTRL_OFFSET    0
#define MSIX_TABLE_OFFSET   4
#define MSIX_PBA_OFFSET     8
// Control
#define MSIX_CTRL_EN            (1<<31) //default 0, msix disabled
#define MSIX_CTRL_FUNC_MASK     (1<<30) //default 0, un masked
#define MSIX_CTRL_TALBE_SHIFT    16
#define MSIX_CTRL_TALBE_MASK     0x3ff
// Table or pba
#define MSIX_BAR_OFFSET_MASK     0xfffffff8 // bit 31:3
#define MSIX_BIR_OFFSET_MASK     0x00000007 // bit 2:0

// MSI-X Table related defines
// 16 Bytes (4 DWORD) for each entry
#define MSIX_TABLE_ENTRY_SIZE    16
// When set to 1, function is prohibited from sending a message
#define MSIX_TABLE_ENTRY_MASK    1 // bit 0
// Pending bits
#define MSIX_PBA_PENDING(i)      (1<<i)

// Pci Command Bits
#define PCI_COMMAND_MASK            0x07
#define PCI_COMMAND_IO_SPACE_EN     0x01
#define PCI_COMMAND_MEM_SPACE_EN    0x02
#define PCI_COMMAND_BUS_MASTER_EN   0x04

// VendorId
#define VENDOR_ID_LW          0x10de
#define VENDOR_ID_3COM        0x10b7
#define VENDOR_ID_INTEL       0x8086
#define VENDOR_ID_REALTEK     0x8086
#define VENDOR_ID_BROADCOM    0x14E4
#define VENDOR_ID_MARVELL     0x1148
#define VENDOR_ID_NEC         0x1033
#define VENDOR_ID_IBM         0x1014
#define VENDOR_ID_AMD         0x1022

//******************************************************************************
//   Class Code
//******************************************************************************
#define CLASS_CODE_AHCI    0x010601
#define CLASS_CODE_AHCI_RAID 0x010400
#define CLASS_CODE_ATA     0x01018a
#define CLASS_CODE_ADMA    0x010185
#define CLASS_CODE_LWMHCI  0x010801
#define CLASS_CODE_AZA     0x040300
#define CLASS_CODE_SATA    0x010185
#define CLASS_CODE_SATA_RAID 0x010485
#define CLASS_CODE_ACI     0x040100
#define CLASS_CODE_ACI_2   0x010601
#define CLASS_CODE_MAC     0x020000
#define CLASS_CODE_BRG     0x068000
#define CLASS_CODE_MCI     0x070300
#define CLASS_CODE_USB     0x0c0310
#define CLASS_CODE_USB2    0x0c0320
#define CLASS_CODE_XHCI    0x0C0330
#define CLASS_CODE_XUSB    0x0C03FE     // XHCI device mods
#define CLASS_CODE_USBCD   0x050100
#define CLASS_CODE_SMB     0x0c0500
#define CLASS_CODE_SMB2    0x0c0500
#define CLASS_CODE_FW      0x0c0010
#define CLASS_CODE_P2P     0x060400
#define CLASS_CODE_P2P_1   0x060401
#define CLASS_CODE_LPC     0x060100
#define CLASS_CODE_SLV     0x058000
#define CLASS_CODE_MEM_CTL 0x050000 //for IO55 (mcp55)
#define CLASS_CODE_LDT     0x060000 // for Ck8
#define CLASS_CODE_PMU     0x0b4000
#define CLASS_CODE_IGPU    0x030000
#define CLASS_CODE_CR_CPU  0x060000 // MCP73
// For CK8S Only
#define CLASS_CODE_PCA     0x0b4000
// General
#define CLASS_CODE_HB      0x060000 // for host bridge

//******************************************************************************
//   DeviceIds
//******************************************************************************
// Mcp1
#define DEVICE_ID_MCP1_APU    0x01b0
#define DEVICE_ID_MCP1_ACI    0x01b1
#define DEVICE_ID_MCP1_LPC    0x01b2
#define DEVICE_ID_MCP1_SMB    0x01b4
#define DEVICE_ID_MCP1_P2P    0x01b8
#define DEVICE_ID_MCP1_ATA    0x01bc
#define DEVICE_ID_MCP1_MCI    0x01c1
#define DEVICE_ID_MCP1_USB    0x01c2
#define DEVICE_ID_MCP1_MAC    0x01c3

// Mcp2
#define DEVICE_ID_MCP2_LPC    0x0060
#define DEVICE_ID_MCP2_SMB2   0x0064
#define DEVICE_ID_MCP2_ATA    0x0065
#define DEVICE_ID_MCP2_MAC    0x0066
#define DEVICE_ID_MCP2_USB    0x0067
#define DEVICE_ID_MCP2_USB2   0x0068
#define DEVICE_ID_MCP2_MCI    0x0069
#define DEVICE_ID_MCP2_ACI    0x006a
#define DEVICE_ID_MCP2_APU    0x006b
#define DEVICE_ID_MCP2_P2P    0x006c
#define DEVICE_ID_MCP2_P2PINT 0x006d
#define DEVICE_ID_MCP2_FW     0x006e
#define DEVICE_ID_3COM_MAC    0x9201

// Ck8
#define DEVICE_ID_CK8_LPC     0x00d0
#define DEVICE_ID_CK8_LDT     0x00d1
#define DEVICE_ID_CK8_APC     0x00d2
#define DEVICE_ID_CK8_SMB2    0x00d4
#define DEVICE_ID_CK8_ATA     0x00d5
#define DEVICE_ID_CK8_MAC     0x00d6
#define DEVICE_ID_CK8_USB     0x00d7
#define DEVICE_ID_CK8_USB2    0x00d8
#define DEVICE_ID_CK8_MCI     0x00d9
#define DEVICE_ID_CK8_ACI     0x00da
#define DEVICE_ID_CK8_APU     0x00db
#define DEVICE_ID_CK8_P2P     0x00dd

// Mcp2s
#define DEVICE_ID_MCP2S_LPC   0x0080
#define DEVICE_ID_MCP2S_SMB2  0x0084
#define DEVICE_ID_MCP2S_ATA   0x0085
#define DEVICE_ID_MCP2S_MAC   0x0086
#define DEVICE_ID_MCP2S_USB   0x0087
#define DEVICE_ID_MCP2S_USB2  0x0088
#define DEVICE_ID_MCP2S_MCI   0x0089
#define DEVICE_ID_MCP2S_ACI   0x008a
#define DEVICE_ID_MCP2S_P2P   0x008b
#define DEVICE_ID_MCP2S_BRG   0x008c
#define DEVICE_ID_MCP2S_SATA  0x008e

// Ck8s
#define DEVICE_ID_CK8S_LPC    0x00e0
#define DEVICE_ID_CK8S_LDT    0x00e1
#define DEVICE_ID_CK8S_APC    0x00e2
#define DEVICE_ID_CK8S_SMB2   0x00e4
#define DEVICE_ID_CK8S_ATA    0x00e5
#define DEVICE_ID_CK8S_MAC    0x00e6
#define DEVICE_ID_CK8S_BRG    0x00df
#define DEVICE_ID_CK8S_USB    0x00e7
#define DEVICE_ID_CK8S_USB2   0x00e8
#define DEVICE_ID_CK8S_MCI    0x00e9
#define DEVICE_ID_CK8S_ACI    0x00ea
#define DEVICE_ID_CK8S_PCA    0x00ec // optional
#define DEVICE_ID_CK8S_P2P    0x00ed
#define DEVICE_ID_CK8S_SATA0  0x00ee
#define DEVICE_ID_CK8S_SATA1  0x00e3

// Mcp04
#define DEVICE_ID_MCP04_LPC   0x0030
#define DEVICE_ID_MCP04_SMB2  0x0034
#define DEVICE_ID_MCP04_ATA   0x0035
#define DEVICE_ID_MCP04_SATA0 0x0036
#define DEVICE_ID_MCP04_SATA1 0x003e // ADMA as well
#define DEVICE_ID_MCP04_MAC   0x0037
#define DEVICE_ID_MCP04_BRG   0x0038
#define DEVICE_ID_MCP04_USB   0x003b
#define DEVICE_ID_MCP04_USB2  0x003c
#define DEVICE_ID_MCP04_MCI   0x0039
#define DEVICE_ID_MCP04_ACI   0x003a
#define DEVICE_ID_MCP04_P2P   0x003d
#define DEVICE_ID_C19_PCIE    0x007e

// Ck804
// three blow are the same block
#define DEVICE_ID_CK804_LPC   0x0050 // Normal mode
#define DEVICE_ID_CK804_LPC_P 0x0051 // Pro mode
#define DEVICE_ID_CK804_SLV   0x00d3 // Slave Mode -- class code changed to 0x5800
#define DEVICE_ID_CK804_LDT   0x005e
#define DEVICE_ID_CK804_SMB2  0x0052
#define DEVICE_ID_CK804_ATA   0x0053
#define DEVICE_ID_CK804_SATA0 0x0054
#define DEVICE_ID_CK804_SATA1 0x0055 // ADMA as well
#define DEVICE_ID_CK804_MAC   0x0056
#define DEVICE_ID_CK804_BRG   0x0057
#define DEVICE_ID_CK804_USB   0x005a
#define DEVICE_ID_CK804_USB2  0x005b
#define DEVICE_ID_CK804_MCI   0x0058
#define DEVICE_ID_CK804_ACI   0x0059
#define DEVICE_ID_CK804_P2P   0x005c
#define DEVICE_ID_CK804_PCIE  0x005d

// Mcp51
#define DEVICE_ID_MCP51_LPC(i) (0x0260+i) // i= 0-3
#define DEVICE_ID_MCP51_LDT   0x0270
#define DEVICE_ID_MCP51_SMB2  0x0264
#define DEVICE_ID_MCP51_ATA   0x0265
#define DEVICE_ID_MCP51_SATA0 0x0266
#define DEVICE_ID_MCP51_SATA1 0x0267
#define DEVICE_ID_MCP51_MAC   0x0268
#define DEVICE_ID_MCP51_BRG   0x0269
#define DEVICE_ID_MCP51_USB   0x026d
#define DEVICE_ID_MCP51_USB2  0x026e
#define DEVICE_ID_MCP51_MCI   0x026a
#define DEVICE_ID_MCP51_ACI   0x026b
#define DEVICE_ID_MCP51_AZA   0x026c
#define DEVICE_ID_MCP51_P2P_1 0x026f
#define DEVICE_ID_MCP51_PMU   0x0271

// Mcp55
#define DEVICE_ID_MCP55_LPC(i) (0x0360+i) // i= 0-7. Different mode gives different ID
#define DEVICE_ID_MCP55_SLV   0x0371 // Slave Mode -- class code changed to 0x5000
#define DEVICE_ID_MCP55_LDT   0x0369
#define DEVICE_ID_MCP55_SMB2  0x0368
#define DEVICE_ID_MCP55_ATA   0x036e
#define DEVICE_ID_MCP55_SATA_OEM  0x037e // There are 3 identical Sata, NO_RAID
#define DEVICE_ID_MCP55_SATA_NON_OEM     0x037f // There are 3 identical Sata, RAID
#define DEVICE_ID_MCP55_MAC   0x0372 // There are 2 macs
#define DEVICE_ID_MCP55_BRG   0x0373 // There are 2 macs
#define DEVICE_ID_MCP55_USB   0x036c
#define DEVICE_ID_MCP55_USB2  0x036d
#define DEVICE_ID_MCP55_AZA   0x0371
#define DEVICE_ID_MCP55_P2P_1 0x0370
#define DEVICE_ID_MCP55_PMU   0x036b
#define DEVICE_ID_MCP55_PCIE_0  0x00374
#define DEVICE_ID_MCP55_PCIE_1  0x00374
#define DEVICE_ID_MCP55_PCIE_2  0x00375
#define DEVICE_ID_MCP55_PCIE_3  0x00376
#define DEVICE_ID_MCP55_PCIE_4  0x00377
#define DEVICE_ID_MCP55_PCIE_5  0x00377

// Mcp61
#define DEVICE_ID_MCP61_LPC(i) (0x03e0+i) // i= 0-7. Different mode gives different ID
#define DEVICE_ID_MCP61_LDT   0x03ea
#define DEVICE_ID_MCP61_SMB2  0x03eb
#define DEVICE_ID_MCP61_ATA   0x03ec
#define DEVICE_ID_MCP61_SATA_OEM  0x03f6 // There are 2 identical Sata
#define DEVICE_ID_MCP61_SATA_CHN  0x03f7 // There are 2 identical Sata
#define DEVICE_ID_MCP61_SATA_NBP  0x03e7 // There are 2 identical Sata
#define DEVICE_ID_MCP61_MAC   0x03ee
#define DEVICE_ID_MCP61_BRG   0x03ef
#define DEVICE_ID_MCP61_MAC_NBP   0x03e5
#define DEVICE_ID_MCP61_BRG_NBP   0x03e6
#define DEVICE_ID_MCP61_USB   0x03f1
#define DEVICE_ID_MCP61_USB2  0x03f2
#define DEVICE_ID_MCP61_AZA   0x03f0
#define DEVICE_ID_MCP61_AZA_NBP 0x03e4
#define DEVICE_ID_MCP61_P2P_1 0x03f3
#define DEVICE_ID_MCP61_PMU   0x03f4
#define DEVICE_ID_MCP61_PCIE_0  0x03e9
#define DEVICE_ID_MCP61_PCIE_1  0x03e9
#define DEVICE_ID_MCP61_PCIE_2  0x03e9

// Mcp65
#define DEVICE_ID_MCP65_LPC(i) (0x0440+i) // i= 0-3. Different mode gives different ID
#define DEVICE_ID_MCP65_LDT   0x0444
#define DEVICE_ID_MCP65_SMB2  0x0446
#define DEVICE_ID_MCP65_ATA   0x0448
#define DEVICE_ID_MCP65_SATA  0x044c // There are 2 identical Sata
#define DEVICE_ID_MCP65_SATA_NEW  0x045d
#define DEVICE_ID_MCP65_AHCI_OEM  0x044c // Native AHCI
#define DEVICE_ID_MCP65_AHCI_CHN  0x044d // Channel Raid 0/1/0+1/5
#define DEVICE_ID_MCP65_AHCI_NBP  0x044e //
#define DEVICE_ID_MCP65_AHCI_NU   0x044f // Not used
#define DEVICE_ID_MCP65_MAC       0x0450
#define DEVICE_ID_MCP65_BRG       0x0452
#define DEVICE_ID_MCP65_MAC_NBP   0x0451
#define DEVICE_ID_MCP65_BRG_NBP   0x0453
//#define DEVICE_ID_MCP65_USB(i)   (0x0454+(4*i)) // i = 0, 1
//#define DEVICE_ID_MCP65_USB2(i)  (0x0455+(2*i)) // i = 0, 1
#define DEVICE_ID_MCP65_USB      0x0454   // Temp will delete later
#define DEVICE_ID_MCP65_USB2     0x0455   // Temp will delete later
#define DEVICE_ID_MCP65_USB_EXT  0x0456   // Temp will delete later
#define DEVICE_ID_MCP65_USB2_EXT 0x0457   // Temp will delete later
#define DEVICE_ID_MCP65_AZA      0x044a
#define DEVICE_ID_MCP65_AZA_NBP  0x044b
#define DEVICE_ID_MCP65_P2P_1 0x0449
#define DEVICE_ID_MCP65_PMU   0x0447
#define DEVICE_ID_MCP65_PCIE_0  0x045a // There are 2 of x2
#define DEVICE_ID_MCP65_PCIE_1  0x045a // There are 2 of x2
#define DEVICE_ID_MCP65_PCIE_2  0x0458 // 1 x8
#define DEVICE_ID_MCP65_PCIE_3  0x0459 // 1 x16

// Mcp67
// The block diagram is horribly unclear right now. Some of these
// may change in the future, partilwlarly as plans for NBP shape up.
#define DEVICE_ID_MCP67_LPC(i)       (0x0548+i) // i=0-3
#define DEVICE_ID_MCP67_SMB2         0x0542
#define DEVICE_ID_MCP67_LDT(i)       (0x0544+i) // i=0-3
#define DEVICE_ID_MCP67_PMU          0x0543     // a.k.a. SMU
#define DEVICE_ID_MCP67_USB(i)       (i ? 0x565 : 0x055e)
#define DEVICE_ID_MCP67_USB2(i)      (i ? 0x564 : 0x055f)
#define DEVICE_ID_MCP67_ATA          0x0560     // a.k.a. IDE
#define DEVICE_ID_MCP67_AZA(i)       (0x055c+i) // i=0-1, 1=NBP
#define DEVICE_ID_MCP67_P2P          0x0561
#define DEVICE_ID_MCP67_PCIE_1       0x0563
#define DEVICE_ID_MCP67_PCIE_16      0x0562
#define DEVICE_ID_MCP67_SATA(i)      (0x0550+i) // i=0-3, {OEM, CHN, NBP, Reserved}
#define DEVICE_ID_MCP67_AHCI(i)      (0x554+i) // i=0-3, {OEM, CHN, NBP, Reserved}
#define DEVICE_ID_MCP67_AHCI_RAID(i) (0x558+i) // i=0-3, {OEM, CHN, NBP, Reserved}
#define DEVICE_ID_MCP67_MAC(i)       (0x054c+i) // i=0-1, 1=NBP
#define DEVICE_ID_MCP67_BRG(i)       (0x054e + i) // i=0-1, 1=NBP
#define DEVICE_ID_MCP72_BRG(i)       (0x07a8 + i) // i=0-3, 1=NBP, 2=OEM, 3=CH

// MCP73
#define DEVICE_ID_MCP73_LPC          0x07d7
#define DEVICE_ID_MCP73_SMB2         0x07d8
#define DEVICE_ID_MCP73_PMU          0x07da      // a.k.a. SMU
#define DEVICE_ID_MCP73_USB          0x07fe
#define DEVICE_ID_MCP73_USB2         0x056a
#define DEVICE_ID_MCP73_ATA          0x056c
#define DEVICE_ID_MCP73_AZA(i)       (0x07fc+i)  // i = 0-1
#define DEVICE_ID_MCP73_P2P          0x056d
#define DEVICE_ID_MCP73_PCIE_1       0x056f
#define DEVICE_ID_MCP73_PCIE_16      0x056e
#define DEVICE_ID_MCP73_SATA(i)      (0x07f0+i)  // i = 0-3
#define DEVICE_ID_MCP73_AHCI(i)      (0x07f4+i)  // i = 0-3
#define DEVICE_ID_MCP73_AHCI_RAID(i) (0x07f8+i)  // i = 0-3
#define DEVICE_ID_MCP73_MAC(i)       (0x07dc+i)  // i = 0-1
#define DEVICE_ID_MCP73_BRG(i)       (0x07de +i )  // i = 0-1

// Mcp77
#define DEVICE_ID_MCP77_MAC(i)      (0x760+i) // i=0-3
#define DEVICE_ID_MCP77_BRG(i)      (0x764+i) // i=0-3
#define DEVICE_ID_MCP77_SATA(i)     (0xad0+i) // i=0-3
#define DEVICE_ID_MCP77_AHCI(i)     (0xad4+i) // i=0-3
#define DEVICE_ID_MCP77_AHCI_RAID(i) (0xad8+i) // i=0-3
// It is a bit tricky
#define DEVICE_ID_MCP77_PCIE_TMS0(i) (0x778+i) // i=0-2, 0=x16, 1=0x8
#define DEVICE_ID_MCP77_PCIE_TMS1(i) (0x77a+i) // i=0-2, 0=x1, 1=0x4
#define DEVICE_ID_MCP77_P2P          0x75a
#define DEVICE_ID_MCP77_AZA(i)       (0x774+i) // i=0-3
#define DEVICE_ID_MCP77_ATA          0x759
#define DEVICE_ID_MCP77_USB(i)       ((i==0) ? 0x77b : 0x77d)
#define DEVICE_ID_MCP77_USB2(i)      ((i==0) ? 0x77c : 0x77e)
#define DEVICE_ID_MCP77_USBCD        0x77f
#define DEVICE_ID_MCP77_PMU          0x753
#define DEVICE_ID_MCP77_LDT(i)      (0x754+i) // i=0-3
#define DEVICE_ID_MCP77_SMB          0x752
#define DEVICE_ID_MCP77_LPC(i)      (0x75c+i) // i=0-3

// Mcp79
#define DEVICE_ID_MCP79_MAC(i)      (0xab0+i) // i=0-3
#define DEVICE_ID_MCP79_SATA(i)     (0xab4+i) // i=0-3
#define DEVICE_ID_MCP79_AHCI(i)     (0xab8+i) // i=0-3
#define DEVICE_ID_MCP79_AHCI_RAID(i) (0xabc+i) // i=0-3
#define DEVICE_ID_MCP79_PCIE_TMS0(i) ((i==0) ? 0xac4 : ((i==1) ? 0xac5 : 0xac8)) // i=0-2, 0=x16, 1=0x8, 2=x4
#define DEVICE_ID_MCP79_PCIE_TMS1(i) (0xac6+i) // i=0-1, 0=x4, 1=0x1
#define DEVICE_ID_MCP79_P2P         0xaab
#define DEVICE_ID_MCP79_AZA(i)      (0xac0+i) // i=0-3
#define DEVICE_ID_MCP79_USB(i)      ((i==0) ? 0xaa5 : 0xaa7)
#define DEVICE_ID_MCP79_USB2(i)     ((i==0) ? 0xaa6 : 0xaa9)
#define DEVICE_ID_MCP79_USBCD       0xaaa
#define DEVICE_ID_MCP79_PMU         0xaa3
#define DEVICE_ID_MCP79_LDT(i)      (0x754+i) // i=0-3
#define DEVICE_ID_MCP79_SMB         0xaa2
#define DEVICE_ID_MCP79_LPC(i)      (0xaac+i) // i=0-3

// MCP89
#define DEVICE_ID_MCP89_MAC         (0xd7d)
#define DEVICE_ID_MCP89_SATA(i)     (0xd84+i)  //i=0-3
#define DEVICE_ID_MCP89_AHCI(i)     (0xd88+i)  //i=0-3
#define DEVICE_ID_MCP89_AHCI_RAID(i) (0xd8c+i) //i=0-3
#define DEVICE_ID_MCP89_PCIE_TMS0(i) (0xd98+i) // i=0-2, 0=x4, 1=x2, 2=x1
#define DEVICE_ID_MCP89_PCIE_TMS1   (0xd9b)
#define DEVICE_ID_MCP89_AZA(i)      (0xd94+i) // i=0-3
#define DEVICE_ID_MCP89_USB         (0xd9c)
#define DEVICE_ID_MCP89_USB2        (0xd9d)
#define DEVICE_ID_MCP89_SMB         (0xd79)
#define DEVICE_ID_MCP89_PMU         (0xd7a)
#define DEVICE_ID_MCP89_LPC(i)      (0xd80+i) // i=0-3
#define DEVICE_ID_MCP89_LNX_AHCI(i) (0x580+i)  //i=0x00-0x0f
#define DEVICE_ID_MCP89_LNX_AZA(i)  (0x590+i) // i=0x00-0x0f

//MCP89 emulation unit IDs
#define DEVICE_ID_MCP89_AHCI_EMU(i)  (0xc34+i)  //i=0-3
#define DEVICE_ID_MCP89_SATA_EMU(i)  (0xc38+i)  //i=0-3
#define DEVICE_ID_MCP89_LWMHCI_EMU(i) (0xc48+i) //i=0-3

// MCP8D
#define DEVICE_ID_MCP8D_SATA(i)     (0xf70+i)  //i=0-3
#define DEVICE_ID_MCP8D_AHCI(i)     (0xf74+i)  //i=0-3
#define DEVICE_ID_MCP8D_AHCI_RAID(i) (0xf78+i) //i=0-3
#define DEVICE_ID_MCP8D_PCIE_TMS0(i) (0xf5c+i) // i=0-2, 0=x4, 1=x2, 2=x1
#define DEVICE_ID_MCP8D_PCIE_TMS1   (0xf5f)
#define DEVICE_ID_MCP8D_AZA(i)      (0xf58+i) // i=0-3
#define DEVICE_ID_MCP8D_USB         (0xf60)
#define DEVICE_ID_MCP8D_USB2        (0xf61)
#define DEVICE_ID_MCP8D_SMB         (0xf52)
#define DEVICE_ID_MCP8D_PMU         (0xf55)
#define DEVICE_ID_MCP8D_LPC         (0xf50)
#define DEVICE_ID_MCP8D_XHCI        (0xf62)
#define DEVICE_ID_MCP8D_LNX_AHCI(i) (0x580+i)  //i=0x00-0x0f
#define DEVICE_ID_MCP8D_LNX_AZA(i)  (0x590+i) // i=0x00-0x0f

// C11/C12
#define DEVICE_ID_C11_P2P     0x01b7
#define DEVICE_ID_C11_HB      0x01A4 // Amd
#define DEVICE_ID_C12_HB      0x01A5 // p6

// C17/C18
#define DEVICE_ID_C17_P2P     0x01E8
#define DEVICE_ID_C17_HB      0x01E0 // k7
#define DEVICE_ID_C18_HB      0x01e1 // p4

// C19
#define DEVICE_ID_C19_P2P     0x007e
#define DEVICE_ID_C19_HB(i)   (0x0070+i)
#define DEVICE_ID_C19_MISC_COMN   0x006f

// C50
#define DEVICE_ID_C50_PCIE(i)     (0x02c3+i) // i = 0 - 2
#define DEVICE_ID_C50_MEM_CTRL(i) (0x02b8+i) // i = 0 - 4
//#define DEVICE_ID_C50_IGPU    0x0130 - 0x13f

// C51
#define DEVICE_ID_C51_PCIE(i)     (0x02fb+i) // i = 0 - 2
#define DEVICE_ID_C51_ULDT(i)     (0x02f0+i)  // i = 0 - 7
#define DEVICE_ID_C51_DLDT        0x2fa
#define DEVICE_ID_C51_MISC_PADS   0x27e
#define DEVICE_ID_C51_MISC_COMN   0x2f8
//#define DEVICE_ID_C51_IGPU    0x0350 - 0x35f

// C55
#define DEVICE_ID_C55_CPU(i)         (0x03A0+i) // i=0-7
#define DEVICE_ID_C55_CPU2            0x03A8
#define DEVICE_ID_C55_MISC_P4         0x03AA
#define DEVICE_ID_C55_MISC_COMMON     0x03A9
#define DEVICE_ID_C55_MISC_CLK        0x03AB
#define DEVICE_ID_C55_CR_DLDT2        0x03B5
#define DEVICE_ID_C55_MISC_PADS       0x03B4
#define DEVICE_ID_C55_PCIE_0          0x03B7
#define DEVICE_ID_C55_PCIE_1          0x03B8
#define DEVICE_ID_C55_PCIE_2          0x03BB
#define DEVICE_ID_C55_PCIE_3          0x03B9
#define DEVICE_ID_C55_CR_DLDT         0x03AC
#define DEVICE_ID_C55_CR_EXTMEM_ECC   0x03B6

// T65
#define DEVICE_ID_T65_ULDT_NON_SLI      0x514
#define DEVICE_ID_T65_ULDT_SLI          0x515
#define DEVICE_ID_T65_DLDT              0x471
#define DEVICE_ID_T65_MISC_COMMON       0x473
#define DEVICE_ID_T65_DLDT2             0x476
#define DEVICE_ID_T65_PCIE(i)           (0x510+i) // i = 0 - 3
#define DEVICE_ID_T65_AHCI(i)           (0x47c+i) // i = 0 - 4
#define DEVICE_ID_T65_MAC(i)            (0x478+i) // i = 0 - 1
#define DEVICE_ID_T65_BRG(i)            (0x47a+i) // i = 0 - 1

// C73
#define DEVICE_ID_C73_MEM               0x080f
#define DEVICE_ID_C73_MEM2              0x0810
#define DEVICE_ID_C73_CPU(i)            (0x0800+i) // i = 0 - 7
#define DEVICE_ID_C73_DLDT              0x0808
#define DEVICE_ID_C73_MISC_COMMON       0x080a
#define DEVICE_ID_C73_CPU2              0x080c
#define DEVICE_ID_C73_DLDT2             0x080d
#define DEVICE_ID_C73_MISC_PADS         0x080e
#define DEVICE_ID_C73_PCIE(i)           (0x0815+i) // i = 0 - 4

// GK1xx Azalia
#define DEVICE_ID_GK104_AZA             0x0e0a
#define DEVICE_ID_GK106_AZA             0x0e0b
#define DEVICE_ID_GK107_AZA             0x0e1b
#define DEVICE_ID_GK110_AZA             0x0e1a

// GK20x Azalia
#define DEVICE_ID_GK208_AZA             0x0e0f

// GM10x Azalia
#define DEVICE_ID_GM107_AZA             0x0fbc

// GM2xx Azalia
#define DEVICE_ID_GM200_AZA             0x0fb0
#define DEVICE_ID_GM204_AZA             0x0fbb
#define DEVICE_ID_GM206_AZA             0x0fba

// GP1xx Azalia
#define DEVICE_ID_GP100_AZA             0x0fb1
#define DEVICE_ID_GP102_AZA             0x10ef
#define DEVICE_ID_GP104_AZA             0x10f0
#define DEVICE_ID_GP106_AZA             0x10f1
#define DEVICE_ID_GP107_AZA             0x0fb9
#define DEVICE_ID_GP108_AZA             0x0fb8

#define DEVICE_ID_GV100_AZA             0x10f2

#define DEVICE_ID_TU102_AZA             0x10f7
#define DEVICE_ID_TU104_AZA             0x10f8
#define DEVICE_ID_TU106_AZA             0x10f9
#define DEVICE_ID_TU108_AZA             0x10fb
#define DEVICE_ID_TU116_AZA             0x1aeb
#define DEVICE_ID_TU117_AZA             0x10fa

// Use LW_XVE1_ID_DEVICE_CHIP_AZALIA_xxx going forward

#define MCP_BIT(b) (1<<b)
#define MCP_MASK(a,b) (((1<<(a-b+1))-1)<<b)
#define MCP_VAL(n,a,b) ((n&(MCP_MASK(a,b)))>>(b))
#define MCP_INSERT(n,m,a,b) ( (n&(~MCP_MASK(a,b)))|(m<<b) )

#define MCP_REG_FIELD(r,a,b) ((Get(r)>>a)&((1<<(b-a+1))-1))
#define MCP_REG_INSERT(r,m,a,b) ( (Get(r)&(~MCP_MASK(a,b)))|(m<<a) )

#endif // end of Mcp.h
