/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_XUSBREG_H
#define INCLUDED_XUSBREG_H
#endif

#define XUSB_REG_PARAM0                            0x00000000
#define XUSB_REG_PARAM0_ERSTMAX                    23:16

#define XUSB_REG_DB                                0x00000004
#define XUSB_REG_DB_TRGT                           15:8
#define XUSB_REG_DB_SEQN                           31:16

#define XUSB_REG_ERSS                              0x00000008
#define XUSB_REG_ERSS_0                            15:0
#define XUSB_REG_ERSS_1                            31:16

#define XUSB_REG_ERS0_BA_LO                        0x00000010
#define XUSB_REG_ERS0_BA_LO_OFFSET                 31:4

#define XUSB_REG_ERS0_BA_HI                        0x00000014
#define XUSB_REG_ERS0_BA_HI_OFFSET                 31:0

#define XUSB_REG_ERS1_BA_LO                        0x00000018
#define XUSB_REG_ERS1_BA_LO_OFFSET                 31:4

#define XUSB_REG_ERS1_BA_HI                        0x0000001C
#define XUSB_REG_ERS1_BA_HI_OFFSET                 31:0

#define XUSB_REG_DP_LO                             0x00000020
#define XUSB_REG_DP_LO_OFFSET                      31:4
#define XUSB_REG_DP_EHB                            3:3

#define XUSB_REG_DP_HI                             0x00000024
#define XUSB_REG_DP_HI_OFFSET                      31:0

#define XUSB_REG_EP_LO                             0x00000028
#define XUSB_REG_EP_LO_OFFSET                      31:4
#define XUSB_REG_EP_LO_SEGI                        1:1
#define XUSB_REG_EP_LO_CYCLE_STATE                 0:0

#define XUSB_REG_EP_HI                             0x0000002C
#define XUSB_REG_EP_HI_OFFSET                      31:0

#define XUSB_REG_PARAMS1                           0x00000030
#define XUSB_REG_PARAMS1_RUN                       0:0
#define XUSB_REG_PARAMS1_PSEE                      1:1
#define XUSB_REG_PARAMS1_IE                        4:4
#define XUSB_REG_PARAMS1_SMI_EIE                   5:5
#define XUSB_REG_PARAMS1_SMI_DESS                  6:6
#define XUSB_REG_PARAMS1_DEVADDR                   30:24
#define XUSB_REG_PARAMS1_EN                        31:31

#define XUSB_REG_PARAMS2                           0x00000034
#define XUSB_REG_PARAMS2_RC                        0:0
#define XUSB_REG_PARAMS2_IP                        4:4
#define XUSB_REG_PARAMS2_DEV_SYS_ERR               5:5

#define XUSB_REG_INTR                              0x00000038
#define XUSB_REG_INTR_MI                           15:0
#define XUSB_REG_INTR_MC                           31:16

#define XUSB_REG_PORTSC                            0x0000003C
#define XUSB_REG_PORTSC_CCS                        0:0
#define XUSB_REG_PORTSC_PED                        1:1
#define XUSB_REG_PORTSC_PR                         4:4
#define XUSB_REG_PORTSC_PLS                        8:5
#define XUSB_REG_PORTSC_PLS_VAL_U0                 0
#define XUSB_REG_PORTSC_PLS_VAL_U1                 1
#define XUSB_REG_PORTSC_PLS_VAL_U2                 2
#define XUSB_REG_PORTSC_PLS_VAL_U3                 3
#define XUSB_REG_PORTSC_PLS_VAL_DIS                4
#define XUSB_REG_PORTSC_PLS_VAL_RX_DET             5
#define XUSB_REG_PORTSC_PLS_VAL_INACTIVE           6
#define XUSB_REG_PORTSC_PLS_VAL_POLLING            7
#define XUSB_REG_PORTSC_PLS_VAL_RCV                8
#define XUSB_REG_PORTSC_PLS_VAL_HOT_RST            9
#define XUSB_REG_PORTSC_PLS_VAL_COMPLIANCE         0xa
#define XUSB_REG_PORTSC_PLS_VAL_LOOPBACK           0xb
#define XUSB_REG_PORTSC_PLS_VAL_RESUME             0xf

#define XUSB_REG_PORTSC_PS                         13:10
#define XUSB_REG_PORTSC_HALT_REJECT                15:15
#define XUSB_REG_PORTSC_HALT_LTSSM                 16:16
#define XUSB_REG_PORTSC_LWS                        16:16    // for T124
#define XUSB_REG_PORTSC_CSC                        17:17
#define XUSB_REG_PORTSC_WRC                        19:19
#define XUSB_REG_PORTSC_STCHG_REQ                  20:20
#define XUSB_REG_PORTSC_PRC                        21:21
#define XUSB_REG_PORTSC_PLSC                       22:22
#define XUSB_REG_PORTSC_PCEC                       23:23
#define XUSB_REG_PORTSC_STCHG_STATE                27:24
#define XUSB_REG_PORTSC_WPR                        30:30
#define XUSB_REG_PORTSC_STCHG_INTR_EN              31:31

#define XUSB_REG_EPPR_LO                           0x00000040
#define XUSB_REG_EPPR_LO_OFFSET                    31:4

#define XUSB_REG_EPPR_HI                           0x00000044
#define XUSB_REG_EPPR_HI_OFFSET                    31:0

#define XUSB_REG_FRAMEIDR                          0x00000048
#define XUSB_REG_FRAMEIDR_UFRAMEID                 2:0
#define XUSB_REG_FRAMEIDR_FRAMEID                  13:3

#define XUSB_REG_PARAMS4                           0x0000004C
#define XUSB_REG_PARAMS4_HSL1S                     1:0
#define XUSB_REG_PARAMS4_HSL1RWE                   3:3
#define XUSB_REG_PARAMS4_HSHIRD                    7:4
#define XUSB_REG_PARAMS4_SSU2T                     15:8
#define XUSB_REG_PARAMS4_SSU1T                     23:16
#define XUSB_REG_PARAMS4_SSFLPA                    24:24
#define XUSB_REG_PARAMS4_VAS                       25:25
#define XUSB_REG_PARAMS4_WC                        26:26
#define XUSB_REG_PARAMS4_WDC                       27:27
#define XUSB_REG_PARAMS4_SSU1E                     28:28
#define XUSB_REG_PARAMS4_SSU2E                     29:29
#define XUSB_REG_PARAMS4_SSFRWE                    30:30
#define XUSB_REG_PARAMS4_SSDPR                     31:31

#define XUSB_REG_EPHALTR                           0x00000050
#define XUSB_REG_EPHALTR_EPHALTB                   31:0

#define XUSB_REG_EPPAUSER                          0x00000054
#define XUSB_REG_EPPAUSER_EPPAUSEB                 31:0

#define XUSB_REG_EPLCR                             0x00000058
#define XUSB_REG_EPLCR_EPLCRB                      31:0

#define XUSB_REG_EPSCR                             0x0000005c
#define XUSB_REG_EPSCR_EPSCRB                      31:0

#define XUSB_REG_FCTR                              0x00000060
#define XUSB_REG_FCTR_IFCT                         6:0
#define XUSB_REG_FCTR_IFCE                         7:7
#define XUSB_REG_FCTR_OFCT                         14:8
#define XUSB_REG_FCTR_0FCE                         15:15

#define XUSB_REG_DNR                               0x00000064
#define XUSB_REG_DNR_DNPF                          0:0
#define XUSB_REG_DNR_NT                            7:4
#define XUSB_REG_DNR_DNTSLO_OFFSET                 31:8

#define XUSB_REG_DNR_DNTSHI                        0x00000068
#define XUSB_REG_DNR_DNTSHI_OFFSET                 31:0

// for T124 device mode
#define XUSB_REG_PORTHALT                          0x0000006C
#define XUSB_REG_PORTHALT_HALT_LTSSM               0:0
#define XUSB_REG_PORTHALT_HALT_REJECT              1:1      //w----c
#define XUSB_REG_PORTHALT_STCHG_REQ                20:20    //w----c

#define XUSB_REG_PORT_TESTMODE                     0x0000006C
#define XUSB_REG_PORT_TESTMODE_TESTCTRL            3:0
#define XUSB_DEV_XHCI_PORT_TM_0                    0x00000070

// see also dev_lw_proj__xusb_dev_xhci.ref
#define XUSB_REG_SOFTRST                           0x00000084
#define XUSB_REG_SOFTRST_RESET                     0:0
#define XUSB_REG_SOFTRST_RSTCNT                    15:8

// //hw/ap_t210/ip/stdif/xusb/1.0/manuals/dev_lw_proj__fpci_xusb.ref
#define XUSB_CFG_SSPX_CORE_CNT0                     0x00000610
#define XUSB_CFG_SSPX_CORE_CNT0_PING_TBRST          7:0

