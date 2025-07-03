/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_XHCIREG_H
#define INCLUDED_XHCIREG_H

#define XHCI_REG_TYPE_CAPABILITY                                0x00000000
#define XHCI_REG_TYPE_OPERATIONAL                               0x20000000
#define XHCI_REG_TYPE_DOORBELL                                  0x40000000
#define XHCI_REG_TYPE_RUNTIME                                   0x80000000

// Capability regs
#define XHCI_REG_CAPLENGTH                                      0x00000000
#define XHCI_REG_HCIVERSION                                     0x00000002

#define XHCI_REG_HCSPARAMS1                                     0x00000004
#define XHCI_REG_HCSPARAMS1_MAXSLOTS                                   7:0
#define XHCI_REG_HCSPARAMS1_MAXINTRS                                  17:8
#define XHCI_REG_HCSPARAMS1_MAXPORTS                                 31:24

#define XHCI_REG_HCSPARAMS2                                     0x00000008
#define XHCI_REG_HCSPARAMS2_IST                                        3:0
#define XHCI_REG_HCSPARAMS2_ERSTMAX                                    7:4
#define XHCI_REG_HCSPARAMS2_IOCINTERVAL                               12:8  // obselete from 1.0
#define XHCI_REG_HCSPARAMS2_MAXSCRATCHPADBUFS_HI                     25:21
#define XHCI_REG_HCSPARAMS2_SPR                                      26:26
#define XHCI_REG_HCSPARAMS2_MAXSCRATCHPADBUFS_LO                     31:27

#define XHCI_REG_HCSPARAMS3                                     0x0000000C
#define XHCI_REG_HCSPARAMS3_U1                                         7:0
#define XHCI_REG_HCSPARAMS3_U2                                       31:16

#define XHCI_REG_HCCPARAMS                                      0x00000010
#define XHCI_REG_HCCPARAMS_AC64                                        0:0
#define XHCI_REG_HCCPARAMS_BNC                                         1:1
#define XHCI_REG_HCCPARAMS_CSZ                                         2:2
#define XHCI_REG_HCCPARAMS_PPC                                         3:3
#define XHCI_REG_HCCPARAMS_PIND                                        4:4
#define XHCI_REG_HCCPARAMS_LHRC                                        5:5
#define XHCI_REG_HCCPARAMS_LTC                                         6:6
#define XHCI_REG_HCCPARAMS_NSS                                         7:7
#define XHCI_REG_HCCPARAMS_FSE                                         8:8
#define XHCI_REG_HCCPARAMS_SBD                                         9:9
#define XHCI_REG_HCCPARAMS_MAXPSASIZE                                15:12
#define XHCI_REG_HCCPARAMS_XEPC                                      31:16

#define XHCI_REG_DBOFF                                          0x00000014

#define XHCI_REG_RTSOFF                                         0x00000018

#define XHCI_REG_HCCPARAMS2                                     0x0000001C
#define XHCI_REG_HCCPARAMS2_U3C                                        0:0
#define XHCI_REG_HCCPARAMS2_CMC                                        1:1
#define XHCI_REG_HCCPARAMS2_FSC                                        2:2
#define XHCI_REG_HCCPARAMS2_CTC                                        3:3
#define XHCI_REG_HCCPARAMS2_LEC                                        4:4
#define XHCI_REG_HCCPARAMS2_CIC                                        5:5
#define XHCI_REG_HCCPARAMS2_ETC                                        6:6
#define XHCI_REG_HCCPARAMS2_TSC                                        7:7
#define XHCI_REG_HCCPARAMS2_GSC                                        8:8
#define XHCI_REG_HCCPARAMS2_VTC                                        9:9

// Operational regs
#define XHCI_REG_OPR_USBCMD                  (XHCI_REG_TYPE_OPERATIONAL+0)
#define XHCI_REG_OPR_USBCMD_RS                                         0:0
#define XHCI_REG_OPR_USBCMD_HCRST                                      1:1
#define XHCI_REG_OPR_USBCMD_INTE                                       2:2
#define XHCI_REG_OPR_USBCMD_HSEE                                       3:3
#define XHCI_REG_OPR_USBCMD_LHCRST                                     7:7
#define XHCI_REG_OPR_USBCMD_CSS                                        8:8
#define XHCI_REG_OPR_USBCMD_CRS                                        9:9
#define XHCI_REG_OPR_USBCMD_EWE                                      10:10

#define XHCI_REG_OPR_USBSTS                  (XHCI_REG_TYPE_OPERATIONAL+4)
#define XHCI_REG_OPR_USBSTS_HCH                                        0:0
#define XHCI_REG_OPR_USBSTS_HSE                                        2:2
#define XHCI_REG_OPR_USBSTS_EINT                                       3:3
#define XHCI_REG_OPR_USBSTS_PCD                                        4:4
#define XHCI_REG_OPR_USBSTS_SSS                                        8:8
#define XHCI_REG_OPR_USBSTS_RSS                                        9:9
#define XHCI_REG_OPR_USBSTS_SRE                                      10:10
#define XHCI_REG_OPR_USBSTS_CNR                                      11:11
#define XHCI_REG_OPR_USBSTS_HCE                                      12:12

#define XHCI_REG_OPR_PAGESIZE                (XHCI_REG_TYPE_OPERATIONAL+8)
#define XHCI_REG_OPR_PAGESIZE_PS                                     15:0

#define XHCI_REG_OPR_DNCTRL               (XHCI_REG_TYPE_OPERATIONAL+0x14)
#define XHCI_REG_OPR_DNCTRL_NE                                       15:0

#define XHCI_REG_OPR_CRCR                 (XHCI_REG_TYPE_OPERATIONAL+0x18)
#define XHCI_REG_OPR_CRCR1                (XHCI_REG_TYPE_OPERATIONAL+0x1c)
#define XHCI_REG_OPR_CRCR_RCS                                          0:0
#define XHCI_REG_OPR_CRCR_CS                                           1:1
#define XHCI_REG_OPR_CRCR_CA                                           2:2
#define XHCI_REG_OPR_CRCR_CCR                                          3:3
#define XHCI_REG_OPR_CRCR_CRP_LO                                      31:6
#define XHCI_REG_OPR_CRCR1_CRP_HI                                     31:0

#define XHCI_REG_OPR_DCBAAP_LO            (XHCI_REG_TYPE_OPERATIONAL+0x30)
#define XHCI_REG_OPR_DCBAAP_LO_BITS                                   31:6
#define XHCI_REG_OPR_DCBAAP_HI            (XHCI_REG_TYPE_OPERATIONAL+0x34)
#define XHCI_REG_OPR_DCBAAP_HI_BITS                                   31:0

#define XHCI_REG_OPR_CONFIG               (XHCI_REG_TYPE_OPERATIONAL+0x38)
#define XHCI_REG_OPR_CONFIG_MAXSLOTEN                                  7:0

// Port Status and Control Registers
// bits that should not be set when normal operating
#define XHCI_REG_PORTSC_WORKING_MASK   0x00ff0002

#define XHCI_REG_PORTSC(i)    (XHCI_REG_TYPE_OPERATIONAL+0x400+0x10*(i-1))
#define XHCI_REG_PORTSC_CCS                                            0:0
#define XHCI_REG_PORTSC_PED                                            1:1
#define XHCI_REG_PORTSC_OCA                                            3:3
// Note that when software writes PR bit to a '1', it must also write a '0' to the Port Enable/Disable bit.
// The HCHalted bit in the USBSTS register shall be '0' before software attempts to use this bit.
#define XHCI_REG_PORTSC_PR                                             4:4
#define XHCI_REG_PORTSC_PLS                                            8:5
#define XHCI_REG_PORTSC_PLS_VAL_U0                                       0
#define XHCI_REG_PORTSC_PLS_VAL_U1                                       1
#define XHCI_REG_PORTSC_PLS_VAL_U2                                       2
#define XHCI_REG_PORTSC_PLS_VAL_U3                                       3
#define XHCI_REG_PORTSC_PLS_VAL_L1                                       2
#define XHCI_REG_PORTSC_PLS_VAL_L2                                       3
#define XHCI_REG_PORTSC_PLS_VAL_RESUME                                  15 // USB2 only

#define XHCI_REG_PORTSC_PP                                             9:9
#define XHCI_REG_PORTSC_PORT_SPEED                                   13:10
#define XHCI_REG_PORTSC_PORT_PIC                                     15:14
#define XHCI_REG_PORTSC_PORT_LWS                                     16:16
#define XHCI_REG_PORTSC_CSC                                          17:17
#define XHCI_REG_PORTSC_PEC                                          18:18
// 0.96 only
#define XHCI_REG_PORTSC_WRC                                          19:19
#define XHCI_REG_PORTSC_OCC                                          20:20
#define XHCI_REG_PORTSC_PRC                                          21:21
#define XHCI_REG_PORTSC_PLC                                          22:22
// 0.96 below
#define XHCI_REG_PORTSC_CEC                                          23:23
#define XHCI_REG_PORTSC_WCE                                          25:25
#define XHCI_REG_PORTSC_WDE                                          26:26
#define XHCI_REG_PORTSC_WOE                                          27:27
// 0.96 below two
#define XHCI_REG_PORTSC_DR                                           30:30
#define XHCI_REG_PORTSC_WPR                                          31:31
#define XHCI_REG_PORTPMSC(i)  (XHCI_REG_TYPE_OPERATIONAL+0x404+0x10*(i-1))
#define XHCI_REG_PORTPMSC_U1_TIMEOUT                                   7:0 // for USB3
#define XHCI_REG_PORTPMSC_U2_TIMEOUT                                  15:8 // for USB3
#define XHCI_REG_PORTPMSC_FLA                                        16:16 // for USB3
#define XHCI_REG_PORTPMSC_L1_DEVICE_SLOT                              15:8 // for USB2
#define XHCI_REG_PORTPMSC_L1S                                          2:0 // for USB2 (L1 Status)
#define XHCI_REG_PORTLI(i)    (XHCI_REG_TYPE_OPERATIONAL+0x408+0x10*(i-1)) // USB3.0 only

// Host Controller Runtime Registers
#define XHCI_REG_MFINDEX                         (XHCI_REG_TYPE_RUNTIME+0)
#define XHCI_REG_MFINDEX_MICROFRAME_INDEX                             13:0
#define XHCI_REG_IMAN(i)                 (XHCI_REG_TYPE_RUNTIME+0x20+32*i)
#define XHCI_REG_IMAN_IP                                               0:0
#define XHCI_REG_IMAN_IE                                               1:1
#define XHCI_REG_IMOD(i)                 (XHCI_REG_TYPE_RUNTIME+0x24+32*i)
#define XHCI_REG_IMOD_IMODI                                           15:0
#define XHCI_REG_IMOD_IMODC                                          31:16
#define XHCI_REG_ERSTSZ(i)               (XHCI_REG_TYPE_RUNTIME+0x28+32*i)
#define XHCI_REG_ERSTSZ_BITS                                          15:0
#define XHCI_REG_ERSTBA(i)               (XHCI_REG_TYPE_RUNTIME+0x30+32*i)
#define XHCI_REG_ERSTBA_BITS                                          31:4
#define XHCI_REG_ERSTBA1(i)              (XHCI_REG_TYPE_RUNTIME+0x34+32*i)
#define XHCI_REG_ERSTBA1_BITS                                         31:0
#define XHCI_REG_ERDP(i)                 (XHCI_REG_TYPE_RUNTIME+0x38+32*i)
// following 0.96
#define XHCI_REG_ERDP_DESI                                             2:0
#define XHCI_REG_ERDP_EHB                                              3:3
#define XHCI_REG_ERDP_BITS                                            31:4
#define XHCI_REG_ERDP1(i)                (XHCI_REG_TYPE_RUNTIME+0x3c+32*i)
#define XHCI_REG_ERDP1_BITS                                           31:0

#define XHCI_REG_DOORBELL(i)                (XHCI_REG_TYPE_DOORBELL+0x4*i)
#define XHCI_REG_DOORBELL_TARGET                                       7:0
#define XHCI_REG_DOORBELL_STREAM_ID                                  31:16

// extended capabilities
#define XHCI_REG_XCAP_PROTOCOL_REV_MAJOR                             31:24
#define XHCI_REG_XCAP_PROTOCOL_REV_MINOR                             23:16
#define XHCI_REG_XCAP_PROTOCOL_PORT_OFFSET                             7:0
#define XHCI_REG_XCAP_PROTOCOL_PORT_COUNT                             15:8
#define XHCI_REG_XCAP_PROTOCOL_SLOT_TYPE                               4:0
#define XHCI_REG_XCAP_PROTOCOL_L1C                                   16:16  // obselete from 1.0
#define XHCI_REG_XCAP_PROTOCOL_HSO                                   17:17
#define XHCI_REG_XCAP_PROTOCOL_IHI                                   18:18
#define XHCI_REG_XCAP_PROTOCOL_PSIC                                  31:28
// below for 1.0
#define XHCI_REG_XCAP_PROTOCOL_PSIV                                    3:0
#define XHCI_REG_XCAP_PROTOCOL_PSIE                                    5:4
#define XHCI_REG_XCAP_PROTOCOL_PLT                                     7:6
#define XHCI_REG_XCAP_PROTOCOL_PFD                                     8:8
#define XHCI_REG_XCAP_PROTOCOL_PSIM                                  31:16

#endif      // INCLUDED_XHCIREG_H
