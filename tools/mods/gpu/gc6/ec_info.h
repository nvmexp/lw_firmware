#ifndef _ec_info_h__included_
#define _ec_info_h__included_

/******************************************************************************
Hardware ID:

VENDOR (subject to change)
    This field identifies the vendor of the microcontroller

FAMILY (subject to change)
    This field identifies the microcontroller family

DEVICE_L/DEVICE_H
    These fields are a copy of the microcontroller's device ID register(s)
******************************************************************************/

#define LW_EC_INFO_HW_ID_0                              0x00000000 /* R--1R */
#define LW_EC_INFO_HW_ID_0_VENDOR                              7:0 /* R-IVF */
#define LW_EC_INFO_HW_ID_0_VENDOR_MICROCHIP             0x00000000 /* R-I-V */

#define LW_EC_INFO_HW_ID_1                              0x00000001 /* R--1R */
#define LW_EC_INFO_HW_ID_1_FAMILY                              7:0 /* R-IVF */
#define LW_EC_INFO_HW_ID_1_FAMILY_PIC18                 0x00000000 /* R-I-V */

#define LW_EC_INFO_HW_ID_2                              0x00000002 /* R--1R */
#define LW_EC_INFO_HW_ID_2_DEVICE_L                            7:0 /* R-IVF */

#define LW_EC_INFO_HW_ID_2_PIC18_REV_4_0                       4:0 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_2_0                       7:5 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_2_0_F46K22         0x00000000 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_2_0_LF46K22        0x00000001 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_2_0_F26K22         0x00000002 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_2_0_LF26K22        0x00000003 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_2_0_F45K22         0x00000000 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_2_0_LF45K22        0x00000001 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_2_0_F25K22         0x00000002 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_2_0_LF25K22        0x00000003 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_2_0_F44K22         0x00000000 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_2_0_LF44K22        0x00000001 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_2_0_F24K22         0x00000002 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_2_0_LF24K22        0x00000003 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_2_0_F43K22         0x00000000 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_2_0_LF43K22        0x00000001 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_2_0_F23K22         0x00000002 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_2_0_LF23K22        0x00000003 /*       */

#define LW_EC_INFO_HW_ID_3                              0x00000003 /* R--1R */
#define LW_EC_INFO_HW_ID_3_DEVICE_H                            7:0 /* R-IVF */

#define LW_EC_INFO_HW_ID_2_PIC18_DEV_10_3                      7:0 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_10_3_F46K22        0x00000054 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_10_3_LF46K22       0x00000054 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_10_3_F26K22        0x00000054 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_10_3_LF26K22       0x00000054 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_10_3_F45K22        0x00000055 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_10_3_LF45K22       0x00000055 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_10_3_F25K22        0x00000055 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_10_3_LF25K22       0x00000055 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_10_3_F44K22        0x00000056 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_10_3_LF44K22       0x00000056 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_10_3_F24K22        0x00000056 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_10_3_LF24K22       0x00000056 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_10_3_F43K22        0x00000057 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_10_3_LF43K22       0x00000057 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_10_3_F23K22        0x00000057 /*       */
#define LW_EC_INFO_HW_ID_2_PIC18_DEV_10_3_LF23K22       0x00000057 /*       */

/******************************************************************************
FW_COMMIT

Any changes made to SMBus registers can be committed to the EEPROM. This
will cause new code to be loaded on boot. Prior to a COMMIT, all register
state lives as a shadow copy.

In order to trigger an update to EEPROM, a special key must be written

KEY:
    Value to write in order to trigger an EEPROM update

UPDATE:
    Trigger a copy of shadow registers to EEPROM. The FW will lock out all
    other activity while this happens. No interrupts can be serviced
    at this time.

******************************************************************************/
#define LW_EC_INFO_FW_COMMIT                            0x0000000a /* RW-1R */
#define LW_EC_INFO_FW_COMMIT_KEY                               6:0 /* RWIVF */
#define LW_EC_INFO_FW_COMMIT_KEY_INIT                   0x00000000 /* RWI-V */
#define LW_EC_INFO_FW_COMMIT_KEY_LOAD_ALL_REGS          0x0000005a /* RW--V */
#define LW_EC_INFO_FW_COMMIT_UPDATE                            7:7 /* RWEVF */
#define LW_EC_INFO_FW_COMMIT_UPDATE_DONE                0x00000000 /* RWE-V */
#define LW_EC_INFO_FW_COMMIT_UPDATE_TRIGGER             0x00000001 /* -W--T */
#define LW_EC_INFO_FW_COMMIT_UPDATE_PENDING             0x00000001 /* R---V */

/******************************************************************************
FW_CFG

Change default mode. This corresponds with the DIP settings on the board
FSM_MODE: Select the mode that the FSM runs in.
    _BOOTLOADER: Force-enter the bootloader
    _GC6K: Kepler-Style GC6 Support track
    _GC6M: Maxwell-Style GC6 Support
    _GC6M_SP: Maxwell-Style GC6 Support + Split-Rail
USART_PRINT: Enable printing to USART as a debug feature. On E-Boards, this
             will be observable via USB.
UPDATE: Update the current MODE. Note, this will not take effect until the
            master FSM is in the FULL_PWR state. Report PENDING until the mode
            has been updated, and reflect that value in MODE_SEL

In terms of initialization, this field should reflect the value read on the
CFG DIP switches on the board. The value can be changed after booting and
re-configured after the fact.
******************************************************************************/

#define LW_EC_INFO_FW_CFG                                0x0000000b /* RW-1R */
#define LW_EC_INFO_FW_CFG_FSM_MODE                              3:0 /* RWEVF */
#define LW_EC_INFO_FW_CFG_FSM_MODE_BOOTLOADER            0x00000000 /* RWE-V */
#define LW_EC_INFO_FW_CFG_FSM_MODE_GC6K                  0x00000001 /* RW--V */
#define LW_EC_INFO_FW_CFG_FSM_MODE_GC6M                  0x00000002 /* RW--V */
#define LW_EC_INFO_FW_CFG_FSM_MODE_GC6M_SP               0x00000003 /* RW--V */
#define LW_EC_INFO_FW_CFG_FSM_MODE_RTD3                  0x00000004 /* RW--V */
#define LW_EC_INFO_FW_CFG_USART_PRINT                           4:4 /* RWIVF */
#define LW_EC_INFO_FW_CFG_USART_PRINT_DISABLE            0x00000000 /* RWI-V */
#define LW_EC_INFO_FW_CFG_USART_PRINT_ENABLE             0x00000001 /* RW--V */
#define LW_EC_INFO_FW_CFG_UPDATE                                7:7 /* RWEVF */
#define LW_EC_INFO_FW_CFG_UPDATE_IDLE                    0x00000000 /* RWE-V */
#define LW_EC_INFO_FW_CFG_UPDATE_TRIGGER                 0x00000001 /* -W--T */
#define LW_EC_INFO_FW_CFG_UPDATE_PENDING                 0x00000001 /* R---V */

/*******************************************************************************
FW_CFG_GPIO

Change mode to Either TU102 to Beyond
WAKE_GPIO: Select TU102
    _ISTU102_YES: EC mode is set to TU102
    _ISTU102_NO: EC mode is set to TU104 and beyond
*******************************************************************************/

#define LW_EC_INFO_FW_CFG_WAKE_GPIO                      0x0000000c /* RW-1R */
#define LW_EC_INFO_FW_CFG_WAKE_GPIO_ISTU102                     0:0 /* RWEVF */
#define LW_EC_INFO_FW_CFG_WAKE_GPIO_ISTU102_NO           0x00000000 /* R---V */
#define LW_EC_INFO_FW_CFG_WAKE_GPIO_ISTU102_YES          0x00000001 /* -W--T */
#define LW_EC_INFO_FW_CFG_WAKE_GPIO_UPDATE                      7:7 /* RWEVF */
#define LW_EC_INFO_FW_CFG_WAKE_GPIO_UPDATE_IDLE          0x00000000 /* RWE-V */
#define LW_EC_INFO_FW_CFG_WAKE_GPIO_UPDATE_TRIGGER       0x00000001 /* -W--T */
#define LW_EC_INFO_FW_CFG_WAKE_GPIO_UPDATE_PENDING       0x00000001 /* R---V */

/******************************************************************************
Firmware ID:

VENDOR_L/VENDOR_H
    These fields identifie the vendor of the firmware.  This is primarily used
    to positively identify the presence of the firmware.

DEVICE_L/DEVICE_H (lwrrently unused/undefined)
    These fields identify the class of firmware, which can be used to identify
    basic functionality, interfaces, and register layout.
******************************************************************************/

#define LW_EC_INFO_FW_ID_0                              0x00000010 /* R--1R */
#define LW_EC_INFO_FW_ID_0_VENDOR_L                            7:0 /* R-IVF */
#define LW_EC_INFO_FW_ID_0_VENDOR_L_LWIDIA              0x000000DE /* R-I-V */

#define LW_EC_INFO_FW_ID_1                              0x00000011 /* R--1R */
#define LW_EC_INFO_FW_ID_1_VENDOR_H                            7:0 /* R-IVF */
#define LW_EC_INFO_FW_ID_1_VENDOR_H_LWIDIA              0x00000010 /* R-I-V */

#define LW_EC_INFO_FW_ID_2                              0x00000012 /* R--1R */
#define LW_EC_INFO_FW_ID_2_DEVICE_L                            7:0 /* R-IVF */
#define LW_EC_INFO_FW_ID_2_DEFICE_L_NULL                0x00000000 /* R-I-V */

#define LW_EC_INFO_FW_ID_3                              0x00000013 /* R--1R */
#define LW_EC_INFO_FW_ID_3_DEVICE_H                            7:0 /* R-IVF */
#define LW_EC_INFO_FW_ID_3_DEVICE_H_NULL                0x00000000 /* R-I-V */

/******************************************************************************
Firmware Version:

VERSION_MAJOR/VERSION_MINOR
    These fields indicate the firmware release version.

DATE_STR
    This array of 11 bytes contains the build date from the C __DATE__ macro

TIME_STR
    This array of 8 bytes contains the build time from the C __TIME__ macro

PROJECT_STR
    This array of 4 bytes contains the LW Project (Board) string
******************************************************************************/

#define LW_EC_INFO_FW_VERSION_MINOR                     0x00000014 /* R--1R */
#define LW_EC_INFO_FW_VERSION_MINOR_VALUE                      7:0 /* R-IVF */

#define LW_EC_INFO_FW_VERSION_MAJOR                     0x00000015 /* R--1R */
#define LW_EC_INFO_FW_VERSION_MAJOR_VALUE                      7:0 /* R-IVF */

#define LW_EC_INFO_FW_DATE_STR(i)                 (0x00000018+(i)) /* R--1A */
#define LW_EC_INFO_FW_DATE_STR__SIZE_1                          11 /*       */
#define LW_EC_INFO_FW_DATE_STR_CHAR                            7:0 /* R--VF */

#define LW_EC_INFO_FW_TIME_STR(i)                 (0x00000024+(i)) /* R--1A */
#define LW_EC_INFO_FW_TIME_STR__SIZE_1                           8 /*       */
#define LW_EC_INFO_FW_TIME_STR_CHAR                            7:0 /* R--VF */

#define LW_EC_INFO_FW_PROJECT_STR(i)              (0x0000002c+(i)) /* R--1A */
#define LW_EC_INFO_FW_PROJECT_STR__SIZE_1                        4 /*       */
#define LW_EC_INFO_FW_PROJECT_STR_CHAR                         7:0 /* R--VF */

/******************************************************************************
 * CAP: Capabilities
 *
 * GC6
 *     If TRUE, the firmware supports GC6 entry and exit
 * GC6M
 *     If TRUE, the firmware supports GC6M entry and exit.
 *           Note: it's not possible to use GC6 and GC6M simultaneously.
 *           Please refer to LW_EC_INFO_FW_CFG_FSM_MODE for the current
 *           configured mode.
 * D3_COLD
 *     If TRUE, the firmware supports D3_COLD (and optimus GOLD)
 *
 * GC6_PWEN
 *      If the EC FW supports ACPI-based (DGPU_SEL/DGPU_EN) power
 *      enable/disable requests for GC6 and D3_COLD, set to TRUE. Else False
 *          For E204x and ahead, this will always be true.
 *
 * PWRUP_T1/T2_SUPPORT
 *      Support for the EVENT_ID_GC6_PWRUP_T1/T2 vectors to be run
 *
 * HPD_MODIFY_SUPPORT
 *      Support for EVENT_ID_HPD_ASSERT and EVENT_ID_HPD_DEASSERT actions to be
 *      taken
 *
******************************************************************************/

#define LW_EC_INFO_CAP(i)                         (0x00000030+(i)) /* R--1A */
#define LW_EC_INFO_CAP__SIZE_1                                   4 /*       */

#define LW_EC_INFO_CAP_0                         LW_EC_INFO_CAP(0) /*       */
#define LW_EC_INFO_CAP_0_GC6                                   0:0 /* R-IVF */
#define LW_EC_INFO_CAP_0_GC6_FALSE                      0x00000000 /* R---V */
#define LW_EC_INFO_CAP_0_GC6_TRUE                       0x00000001 /* R-I-V */
#define LW_EC_INFO_CAP_0_GC6_PWEN                              1:1 /* R-IVF */
#define LW_EC_INFO_CAP_0_GC6_PWEN_FALSE                 0x00000000 /* R---V */
#define LW_EC_INFO_CAP_0_GC6_PWEN_TRUE                  0x00000001 /* R-I-V */
#define LW_EC_INFO_CAP_0_GC6M                                  2:2 /* R-IVF */
#define LW_EC_INFO_CAP_0_GC6M_FALSE                     0x00000000 /* R---V */
#define LW_EC_INFO_CAP_0_GC6M_TRUE                      0x00000001 /* R-I-V */
#define LW_EC_INFO_CAP_0_D3_COLD                               3:3 /* R-IVF */
#define LW_EC_INFO_CAP_0_D3_COLD_FALSE                  0x00000000 /* R---V */
#define LW_EC_INFO_CAP_0_D3_COLD_TRUE                   0x00000001 /* R-I-V */

#define LW_EC_INFO_CAP_1                         LW_EC_INFO_CAP(1) /*       */
#define LW_EC_INFO_CAP_1_PWRUP_T1_SUPPORT                      0:0 /* R-IVF */
#define LW_EC_INFO_CAP_1_PWRUP_T1_SUPPORT_FALSE         0x00000000 /* R---V */
#define LW_EC_INFO_CAP_1_PWRUP_T1_SUPPORT_TRUE          0x00000001 /* R-I-V */
#define LW_EC_INFO_CAP_1_PWRUP_T2_SUPPORT                      1:1 /* R-IVF */
#define LW_EC_INFO_CAP_1_PWRUP_T2_SUPPORT_FALSE         0x00000000 /* R---V */
#define LW_EC_INFO_CAP_1_PWRUP_T2_SUPPORT_TRUE          0x00000001 /* R-I-V */
#define LW_EC_INFO_CAP_1_HPD_MODIFY_SUPPORT                    2:2 /* R-IVF */
#define LW_EC_INFO_CAP_1_HPD_MODIFY_SUPPORT_FALSE       0x00000000 /* R---V */
#define LW_EC_INFO_CAP_1_HPD_MODIFY_SUPPORT_TRUE        0x00000001 /* R-I-V */
#define LW_EC_INFO_CAP_1_RTD3_SUPPORT                          3:3 /* R-IVF */
#define LW_EC_INFO_CAP_1_RTD3_SUPPORT_FALSE             0x00000000 /* R---V */
#define LW_EC_INFO_CAP_1_RTD3_SUPPORT_TRUE              0x00000001 /* R-I-V */

/******************************************************************************
 * EC_FSM_EVENT_CNTL: Control Aspects of the Master State Machine in the EC
 *
 *
 * FULL_PWRDN = power down the GPU completely. No need for any external events
 * FULL_PWRUP = power up the GPU completely. No need for any external events
 * GC6_PWRDN_PREP = Once in this state, de-asserting PWR_EN will not trigger
 *                  full powerdown. No actions here: wait for FB_CLAMP_TGL_REQ#
 * GC6_PWRUP_FULL = Exit GC6 for "sparse" refresh. Don't touch FB_CLAMP_TGL_REQ#
 * GC6_PWRUP_SR_UPDATE = Exit GC6 for "burst" refresh. Assert FB_CLAMP_TGL_REQ#
 *                       prior to PEX_RST# de-assert, and hold it for 3mS before
 *                       de-asserting
 * GC6_PWRUP_T1 = Test powerup state for GC6M. Can run some arbitrary power sequencer
 *                code. Requires GPU_EVENT interrupt to exit. T1 used for HPD
 * GC6_PWRUP_T2 = Test powerup state for GC6M. Can run some arbitrary power sequencer
 *                code. Requires GPU_EVENT interrupt to exit. T2 used for HPD_IRQ
 * HPD_ASSERT = Assert HPD signal. LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_1_MISC0
 * HPD_DEASSERT = De-Assert HPD signal. LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_1_MISC0
 *
 * RTD3_PWRDN_PEXRESET_ASSERT = Tell PIC to assert PEX_RESET# required after RTD3 entry
 * RTD3_PWRUP_PEXRESET_DEASSERT = Tell PIC to de-assert PEX_RESET# for RTD3 exit
 *
 * GC6_PWRUP_T1 and T2 should not be enabled unless the board and IOs have been
 * configured properly. Support can be relayed to MODS using the CAP_1 register.
 * See LW_EC_INFO_CAP_1_PWRUP_T1_SUPPORT and LW_EC_INFO_CAP_1_PWRUP_T2_SUPPORT
 *
 *
 * TRIGGER_EVENT = Trigger the event on LW_EC. Poll for EVENT_DONE to signal
 *                  completion
 *****************************************************************************/

#define LW_EC_INFO_FSM_EVENT_CNTL                                0x00000034 /* RW-1R */
#define LW_EC_INFO_FSM_EVENT_CNTL_EVENT_ID                              6:0 /* RWIVF */
#define LW_EC_INFO_FSM_EVENT_CNTL_EVENT_ID_FULL_PWRDN            0x00000000 /* RWI-V */
#define LW_EC_INFO_FSM_EVENT_CNTL_EVENT_ID_FULL_PWRUP            0x00000001 /* RW--V */
#define LW_EC_INFO_FSM_EVENT_CNTL_EVENT_ID_PWRDN_PREP            0x00000004 /* RW--V */
#define LW_EC_INFO_FSM_EVENT_CNTL_EVENT_ID_GC6_PWRDN_PREP        0x00000008 /* RW--V */
#define LW_EC_INFO_FSM_EVENT_CNTL_EVENT_ID_GC6_PWRUP_FULL        0x00000009 /* RW--V */
#define LW_EC_INFO_FSM_EVENT_CNTL_EVENT_ID_GC6_PWRUP_SR_UPDATE   0x0000000a /* RW--V */
#define LW_EC_INFO_FSM_EVENT_CNTL_EVENT_ID_GC6_PWRUP_T1          0x0000000b /* RW--V */
#define LW_EC_INFO_FSM_EVENT_CNTL_EVENT_ID_GC6_PWRUP_T2          0x0000000c /* RW--V */
#define LW_EC_INFO_FSM_EVENT_CNTL_EVENT_ID_HPD_ASSERT            0x00000010 /* RW--V */
#define LW_EC_INFO_FSM_EVENT_CNTL_EVENT_ID_HPD_DEASSERT          0x00000011 /* RW--V */
#define LW_EC_INFO_FSM_EVENT_CNTL_EVENT_ID_RTD3_PWRDN_PEXRESET_ASSERT       0x00000012 /* RW--V */
#define LW_EC_INFO_FSM_EVENT_CNTL_EVENT_ID_RTD3_PWRUP_PEXRESET_DEASSERT     0x00000013 /* RW--V */
#define LW_EC_INFO_FSM_EVENT_CNTL_TRIGGER_EVENT                         7:7 /* RWEVF */
#define LW_EC_INFO_FSM_EVENT_CNTL_TRIGGER_EVENT_READY            0x00000000 /* RWE-V */
#define LW_EC_INFO_FSM_EVENT_CNTL_TRIGGER_EVENT_DONE             0x00000000 /* R---V */
#define LW_EC_INFO_FSM_EVENT_CNTL_TRIGGER_EVENT_TRIGGER          0x00000001 /* -W--T */
#define LW_EC_INFO_FSM_EVENT_CNTL_TRIGGER_EVENT_PENDING          0x00000001 /* R---V */

/******************************************************************************
 * EC_FSM_STATUS_0: Contains status for the Master FSM
 *
 * PWR_STATUS: Current power status. If PWR_SEQ_STATUS_BUSY is true, this
 *             state represents the source state, not the destination state.
 *       GPU_OFF: All Rails are down (cold boot or GOLD)
 *       GPU_ON: All rails are up and the GPU is out of reset
 *       GPU_GC6: GPU is in GC6. FBVDDQ_MEM is on. If split-rail FBVDDQ_GPU is off.
 *                Else, FBVDDQ_GPU is on. Other rails are off. Master FSM
 * PWR_SEQ_STATUS:
 *       IDLE: Power sequencer is not active
 *       BUSY: Power sequencer is lwrrently sequencing
 *
 * MASTER_FSM_STATUS:
 *       IDLE: Master FSM is not active (in D3_COLD, FULL_PWR, GC6, or GC6M)
 *       BUSY: Master FSM is lwrrently transitioning to a terminal state
 *
 *****************************************************************************/

#define LW_EC_INFO_FSM_STATUS_0                             0x00000035 /* R--1R */
#define LW_EC_INFO_FSM_STATUS_0_PWR_STATUS                         2:0 /* R-IVF */
#define LW_EC_INFO_FSM_STATUS_0_PWR_STATUS_GPU_UNKNOWN      0x00000000 /* R-I-V */
#define LW_EC_INFO_FSM_STATUS_0_PWR_STATUS_GPU_ON           0x00000001 /* R---V */
#define LW_EC_INFO_FSM_STATUS_0_PWR_STATUS_GPU_OFF          0x00000002 /* R---V */
#define LW_EC_INFO_FSM_STATUS_0_PWR_STATUS_GPU_GC6          0x00000003 /* R---V */
#define LW_EC_INFO_FSM_STATUS_0_PWR_STATUS_GPU_GC6M         0x00000004 /* R---V */
#define LW_EC_INFO_FSM_STATUS_0_PWR_STATUS_GPU_GC5          0x00000005 /* R---V */
#define LW_EC_INFO_FSM_STATUS_0_PWR_STATUS_GPU_RTD3         0x00000006 /* R---V */
#define LW_EC_INFO_FSM_STATUS_0_PWR_SEQ_STATUS                     3:3 /* R-IVF */
#define LW_EC_INFO_FSM_STATUS_0_PWR_SEQ_STATUS_IDLE         0x00000000 /* R-I-V */
#define LW_EC_INFO_FSM_STATUS_0_PWR_SEQ_STATUS_BUSY         0x00000001 /* R---V */
#define LW_EC_INFO_FSM_STATUS_0_MASTER_FSM_STATUS                  4:4 /* R-IVF */
#define LW_EC_INFO_FSM_STATUS_0_MASTER_FSM_STATUS_IDLE      0x00000000 /* R-I-V */
#define LW_EC_INFO_FSM_STATUS_0_MASTER_FSM_STATUS_BUSY      0x00000001 /* R---V */

/******************************************************************************
 * EC_FSM_STATUS_1: Contains status for the Master FSM
 *
 * WAKE_CAUSE: Reports the first wake event that caused wakeup from GC6,
 *               GC6M, or GOLD
 *
 * TODO - Roll this into the debug register? Seems like we kind of want this
 *        anyways, irrespective of debug
 *
 *****************************************************************************/

#define LW_EC_INFO_FSM_STATUS_1                             0x00000036 /* R--1R */
#define LW_EC_INFO_FSM_STATUS_1_WAKE_CAUSE                         3:0 /* R-IVF */
#define LW_EC_INFO_FSM_STATUS_1_WAKE_CAUSE_NONE             0x00000000 /* R-I-V */
#define LW_EC_INFO_FSM_STATUS_1_WAKE_CAUSE_ACPI_WAKE        0x00000001 /* R---V */
#define LW_EC_INFO_FSM_STATUS_1_WAKE_CAUSE_SMB_WAKE         0x00000002 /* R---V */
#define LW_EC_INFO_FSM_STATUS_1_WAKE_CAUSE_HPD              0x00000003 /* R---V */
#define LW_EC_INFO_FSM_STATUS_1_WAKE_CAUSE_GPU_EVENT        0x00000004 /* R---V */

/******************************************************************************
 * EC_FSM_STATUS_2: Contains Error Status - Cleared after reading
 *
 * IS_ERROR: Did an error occur?
 * ERROR_CODE:
 *  NONE - No Error
 *  ERR_TIMEOUT - Timeout Erorr
 *  ERR_PRI - Read/Write to an invalid address
 *  ERR_GPU_EVENT_PROT - GPU_EVENT protocol error. This would supercede a
 *                       ERR_TMOUT.
 *
 *****************************************************************************/

#define LW_EC_INFO_FSM_STATUS_2                                 0x00000037 /* R--1R */
#define LW_EC_INFO_FSM_STATUS_2_IS_ERROR                               0:0 /* R-IVF */
#define LW_EC_INFO_FSM_STATUS_2_IS_ERROR_NO                     0x00000000 /* R-I-V */
#define LW_EC_INFO_FSM_STATUS_2_IS_ERROR_YES                    0x00000001 /* R---V */
#define LW_EC_INFO_FSM_STATUS_2_ERROR_CODE                             7:1 /* R-IVF */
#define LW_EC_INFO_FSM_STATUS_2_ERROR_CODE_NONE                 0x00000000 /* R-I-V */
#define LW_EC_INFO_FSM_STATUS_2_ERROR_CODE_ERR_TIMEOUT          0x00000001 /* R---V */
#define LW_EC_INFO_FSM_STATUS_2_ERROR_CODE_ERR_PRI              0x00000002 /* R---V */
#define LW_EC_INFO_FSM_STATUS_2_ERROR_CODE_ERR_GPU_EVENT_PROT   0x00000003 /* R---V */

/******************************************************************************
 * LW_EC_INFO_FSM_STATUS_3: Contains status for the Master FSM
 *
 * WAKE#_TOGGLED: Reports if wake# is toggled used for RTD3 exit
 *****************************************************************************/

#define LW_EC_INFO_FSM_STATUS_3                             0x00000038 /* R--1R */
#define LW_EC_INFO_FSM_STATUS_3_RTD3_WAKE_TOGGLED                  0:0 /* R-IVF */
#define LW_EC_INFO_FSM_STATUS_3_RTD3_WAKE_TOGGLED_NO        0x00000000 /* R-I-V */
#define LW_EC_INFO_FSM_STATUS_3_RTD3_WAKE_TOGGLED_YES       0x00000001 /* R-I-V */

/******************************************************************************
 * DEBUG: Contains information to debug the EC firmware.
 *
 * The entire 16-byte range should be read when debugging to account for future
 * and/or temporary additions not dolwmented here.
 *
 * FSM_STATE: Current internal state number
 *****************************************************************************/

#define LW_EC_INFO_DEBUG(i)                           (0x00000040+(i)) /* R--1A */
#define LW_EC_INFO_DEBUG__SIZE_1                                    16 /*       */

/******************************************************************************
* FSM_STATE - Numerical dump for the master FSM state
*
* The Master FSM state machine can be found at:
*
* https://p4viewer.lwpu.com/get///hw/notebook/GPU_Platform_Solutions/Platform_Arch/LW_EC/N15x/lw_ec_design.vsd
*
******************************************************************************/

#define LW_EC_INFO_DEBUG_0                         LW_EC_INFO_DEBUG(0) /*       */
#define LW_EC_INFO_DEBUG_0_FSM_STATE                               7:0 /* R-IVF */
#define LW_EC_INFO_DEBUG_0_FSM_STATE_INIT                   0x00000000 /* R-I-V */
#define LW_EC_INFO_DEBUG_0_FSM_STATE_RESET_NO_PWR           0x00000000 /* R---V */
#define LW_EC_INFO_DEBUG_0_FSM_STATE_RESET                  0x00000001 /* R---V */
#define LW_EC_INFO_DEBUG_0_FSM_STATE_FULL_PWR               0x00000002 /* R---V */
#define LW_EC_INFO_DEBUG_0_FSM_STATE_D3_PWRDN_RST           0x00000003 /* R---V */
#define LW_EC_INFO_DEBUG_0_FSM_STATE_D3_COLD                0x00000004 /* R---V */
#define LW_EC_INFO_DEBUG_0_FSM_STATE_D3_PWRUP_RST           0x00000005 /* R---V */
#define LW_EC_INFO_DEBUG_0_FSM_STATE_GC6_PWRDN_RST          0x00000006 /* R---V */
#define LW_EC_INFO_DEBUG_0_FSM_STATE_GC6                    0x00000007 /* R---V */
#define LW_EC_INFO_DEBUG_0_FSM_STATE_GC6_PWRUP_REQ          0x00000008 /* R---V */
#define LW_EC_INFO_DEBUG_0_FSM_STATE_GC6M_PWRDN_RST         0x00000009 /* R---V */
#define LW_EC_INFO_DEBUG_0_FSM_STATE_GC6M                   0x0000000a /* R---V */
#define LW_EC_INFO_DEBUG_0_FSM_STATE_GC6M_PWRUP_REQ         0x0000000b /* R---V */
#define LW_EC_INFO_DEBUG_0_FSM_STATE_GC6M_PWRUP_SR_UPD      0x0000000c /* R---V */
#define LW_EC_INFO_DEBUG_0_FSM_STATE_GC6M_PWRUP_T1          0x0000000d /* R---V */
#define LW_EC_INFO_DEBUG_0_FSM_STATE_GC6M_PWRUP_T2          0x0000000e /* R---V */

/******************************************************************************
* PWR_SEQ_PC: Program counter that the power sequencer is lwrrently on.
*               Note - check LW_EC_INFO_FSM_STATUS_0_PWR_SEQ_STATUS to see if
*               the sequnecer is busy to find out if it's hung.
*
******************************************************************************/

#define LW_EC_INFO_DEBUG_1                         LW_EC_INFO_DEBUG(1) /*       */
#define LW_EC_INFO_DEBUG_1_PWR_SEQ_PC                              7:0 /* R-IVF */
#define LW_EC_INFO_DEBUG_1_PWR_SEQ_PC_INIT                  0x00000000 /* R-I-V */

/******************************************************************************
* PWR_SEQ_INST: Power sequencer current instruction. Get the decode at
*               LW_EC_INFO_PWR_SEQ_INST
******************************************************************************/

#define LW_EC_INFO_DEBUG_2                         LW_EC_INFO_DEBUG(2) /*       */
#define LW_EC_INFO_DEBUG_2_PWR_SEQ_INST                            7:0 /* R-IVF */
#define LW_EC_INFO_DEBUG_2_PWR_SEQ_INST_INIT                0x00000000 /* R-I-V */

/******************************************************************************
* ENTRY_MS: Time (in miliseconds) from the time the master FSM leaves FULL_PWR
*           to the time it ends up in another terminal state (GC6,GC6M,D3_COLD)
*
******************************************************************************/

#define LW_EC_INFO_DEBUG_3                         LW_EC_INFO_DEBUG(3) /*       */
#define LW_EC_INFO_DEBUG_3_ENTRY_MS                                7:0 /* R-IVF */
#define LW_EC_INFO_DEBUG_3_ENTRY_MS_INIT                    0x00000000 /* R-I-V */

/******************************************************************************
* EXIT_MS: Time (in miliseconds) from the time the master fsm gets a wake
*           interrupt to the time it arrives back in the FULL_PWR state. *
******************************************************************************/

#define LW_EC_INFO_DEBUG_4                         LW_EC_INFO_DEBUG(4) /*       */
#define LW_EC_INFO_DEBUG_4_EXIT_MS                                 7:0 /* R-IVF */
#define LW_EC_INFO_DEBUG_4_EXIT_MS_INIT                     0x00000000 /* R-I-V */

/********************************************************************************
 *LW_EC_INFO_GPIO_OUTPUT_CNTL: GPIO Output Control. Map GPIO designator pins
 *                             to functions. TODO: Need XBAR to assign to
 *                             designator pins? Might be better to fix
 *                             relationship via a header file.
 *
 * SEL: XBAR select. Picks a GPIO function to map to the GPIO.
 *      _NORMAL: Under complete SW control
 *      _FB_CLAMP: FBC State Machine controls this
 *      _BOARD_PGOOD: 1, if the equation in LW_EC_INFO_PGOOD_CNTL is satisfied.
 *      _DBG_PWR_SEQ_BUSY: 1 when the power sequencer is busy
 *      _DBG_FSM_BUSY: 1 when the master FSM is busy
 *      _DBG_WAKE_INTR_RCVD 1, when a wake interrupt comes.
 *      _DBG_FSM_ERROR: 1, when an error is encountered.
 *      _PWR_SEQ_*: Under power sequencer control.
 *          _VR_EN_3V3:        3V3 Regulator Enable
 *          _VR_EN_LWVDD:      LWVDD Regulator Enable
 *          _VR_EN_VRAM_TOTAL: DRAM FBVDD/Q Regulator Enable
 *          _VR_EN_GPU_FBIO:   GPU-side FBVDDQ Regulator Enable
 *          _VR_EN_1V:         1V (PEXVDD) Regulator Enable
 *          _EC_GPU_RST:       GPU_RST* Control.
 *          _PGOOD:            Board PGOOD
 *          _PWR_ALERT:        Usually mapped to GPIO12 on the board.
 *          _MISC*:            MISC define for unknown sequencer outputs
 *      _PWM_0: PWM Generator 0
 *      _PWM_1: PWM Generator 1
 *
 * NORM_OUT_DATA: When SEL = NORMAL, Set this bit to control the assertion or
 *                De-assertion of the signal. Applies BEFORE any polarity is
 *                selected
 *
 * OUT_STATE: The state at the pin. 1 is 3.3V and 0 is 0V.
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_OUTPUT_CNTL(i)                    (0x00000050 + (i)*2) /* RW-1A */
#define LW_EC_INFO_GPIO_OUTPUT_CNTL__SIZE_1                                 32 /*       */
#define LW_EC_INFO_GPIO_OUTPUT_CNTL_SEL                                    5:0 /* RWIVF */
#define LW_EC_INFO_GPIO_OUTPUT_CNTL_SEL_NORMAL                      0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_OUTPUT_CNTL_SEL_FB_CLAMP                    0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_CNTL_SEL_BOARD_PGOOD                 0x00000002 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_CNTL_SEL_DBG_PWR_SEQ_BUSY            0x00000008 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_CNTL_SEL_DBG_FSM_BUSY                0x00000009 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_CNTL_SEL_DBG_FSM_WAKE_INTR_RCVD      0x0000000a /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_CNTL_SEL_DBG_FSM_ERROR               0x0000000b /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_CNTL_SEL_PWR_SEQ_GPU_EVENT           0x00000010 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_CNTL_SEL_PWR_SEQ_VR_EN_3V3           0x00000011 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_CNTL_SEL_PWR_SEQ_VR_EN_LWVDD         0x00000012 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_CNTL_SEL_PWR_SEQ_VR_EN_VRAM_TOTAL    0x00000013 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_CNTL_SEL_PWR_SEQ_VR_EN_GPU_FBIO      0x00000014 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_CNTL_SEL_PWR_SEQ_VR_EN_1V            0x00000015 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_CNTL_SEL_PWR_SEQ_EC_GPU_RST          0x00000016 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_CNTL_SEL_PWR_SEQ_PGOOD               0x00000017 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_CNTL_SEL_PWR_SEQ_PWR_ALERT           0x00000018 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_CNTL_SEL_PWR_SEQ_MISC0               0x00000019 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_CNTL_SEL_PWR_SEQ_MISC1               0x0000001a /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_CNTL_SEL_PWM_0                       0x00000020 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_CNTL_SEL_PWM_1                       0x00000021 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_CNTL_NORM_OUT_DATA                          6:6 /* RWIVF */
#define LW_EC_INFO_GPIO_OUTPUT_CNTL_NORM_OUT_DATA_DEASSERT          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_OUTPUT_CNTL_NORM_OUT_DATA_ASSERT            0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_CNTL_OUT_STATE                              7:7 /* R-IVF */
#define LW_EC_INFO_GPIO_OUTPUT_CNTL_OUT_DATA_LOW                    0x00000000 /* R---V */
#define LW_EC_INFO_GPIO_OUTPUT_CNTL_OUT_DATA_HIGH                   0x00000001 /* R-I-V */

/* *****************************************************************************
* LW_EC_INF_GPIO_OUTPUT_CFG: Set up for the GPIO output controls
*
* DRIVE_MODE: Descriptions of each below. Assess Tri-state AFTER ilwersion
*   DISABLED: Tri-State the output
*   FULL_DRIVE: Output is always enabled. Drive hard 1 and hard 0
*   OPEN_DRAIN: Tri-state if driving 1. Drive hard 0 (weak pullup on board)
*   OPEN_SOURCE: Tri-state if driving 0. Drive hard 1 (weak pulldown on board)
*
* COLLISION_AWARE: If GPIO is supposed to be set to ASSERT (logical), check the
*                  port state first. If the port state is already set to
*                  ASSERT, do not update the drive value.
*
* POLARITY: Determine the ilwersion of the output. Apply polarity BEFORE
*           applying drive-mode.
*   ACTIVE_LOW: ASSERT is 0, DEASSERT is 1
*   ACTIVE_HIGH ASSERT is 1, DEASSERT is 0
* *****************************************************************************/

#define LW_EC_INFO_GPIO_OUTPUT_CFG(i)                     (0x00000051 + (i)*2) /* RW-1A */
#define LW_EC_INFO_GPIO_OUTPUT_CFG__SIZE_1                                  32 /*       */
#define LW_EC_INFO_GPIO_OUTPUT_CFG_DRIVE_MODE                              1:0 /* RWIVF */
#define LW_EC_INFO_GPIO_OUTPUT_CFG_DRIVE_MODE_DISABLED              0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_OUTPUT_CFG_DRIVE_MODE_FULL_DRIVE            0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_CFG_DRIVE_MODE_OPEN_DRAIN            0x00000002 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_CFG_DRIVE_MODE_OPEN_SOURCE           0x00000003 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_CFG_COLLISION_AWARE                         2:2 /* RWIVF */
#define LW_EC_INFO_GPIO_OUTPUT_CFG_COLLISION_AWARE_NO               0x00000000 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_CFG_COLLISION_AWARE_YES              0x00000001 /* RWI-V */
#define LW_EC_INFO_GPIO_OUTPUT_CFG_POLARITY                                3:3 /* RWIVF */
#define LW_EC_INFO_GPIO_OUTPUT_CFG_POLARITY_ACTIVE_LOW              0x00000000 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_CFG_POLARITY_ACTIVE_HIGH             0x00000001 /* RWI-V */

/******************************************************************************
* GPIO_OUTPUT_DEFINE_BOARD_PGOOD - PGOOD is a special ouptut spot
*
* BOARD_PGOOD is a special output, in that it's components can be indivdually
* enabled. BOARD_PGOOD = 1 only when all the inputs that are set to ENABLED
* are asserted.
*
* The following input signals can be enabled:
*   PWR_SRC_PGOOD
*   3V3_PGOOD
*   LWVDD_PGOOD
*   1V_PGOOD
*   GPU_FBIO_PGOOD
*   VRAM_TOTAL_PGOOD
*   GPU_RST
*   GPU_RST_HOLD
*   GC5_MODE
*   GC6_FB_EN
*
*
******************************************************************************/
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_0_BOARD_PGOOD                             0x00000090 /* RW-1R */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_0_BOARD_PGOOD_PWR_SRC_PGOOD                      0:0 /* RWIVF */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_0_BOARD_PGOOD_PWR_SRC_PGOOD_DISABLED      0x00000000 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_0_BOARD_PGOOD_PWR_SRC_PGOOD_ENABLED       0x00000001 /* RWI-V */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_0_BOARD_PGOOD_3V3_PGOOD                          1:1 /* RWIVF */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_0_BOARD_PGOOD_3V3_PGOOD_DISABLED          0x00000000 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_0_BOARD_PGOOD_3V3_PGOOD_ENABLED           0x00000001 /* RWI-V */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_0_BOARD_PGOOD_LWVDD_PGOOD                        2:2 /* RWIVF */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_0_BOARD_PGOOD_LWVDD_PGOOD_DISABLED        0x00000000 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_0_BOARD_PGOOD_LWVDD_PGOOD_ENABLED         0x00000001 /* RWI-V */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_0_BOARD_PGOOD_1V_PGOOD                           3:3 /* RWIVF */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_0_BOARD_PGOOD_1V_PGOOD_DISABLED           0x00000000 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_0_BOARD_PGOOD_1V_PGOOD_ENABLED            0x00000001 /* RWI-V */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_0_BOARD_PGOOD_GPU_FBIO_PGOOD                     4:4 /* RWIVF */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_0_BOARD_PGOOD_GPU_FBIO_PGOOD_DISABLED     0x00000000 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_0_BOARD_PGOOD_GPU_FBIO_PGOOD_ENABLED      0x00000001 /* RWI-V */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_0_BOARD_PGOOD_VRAM_TOTAL_PGOOD                   5:5 /* RWIVF */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_0_BOARD_PGOOD_VRAM_TOTAL_PGOOD_DISABLED   0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_0_BOARD_PGOOD_VRAM_TOTAL_PGOOD_ENABLED    0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_0_BOARD_PGOOD_3V3_PIC_PGOOD                      6:6 /* RWIVF */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_0_BOARD_PGOOD_3V3_PIC_PGOOD_DISABLED      0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_0_BOARD_PGOOD_3V3_PIC_PGOOD_ENABLED       0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_0_BOARD_PGOOD_GPU_RST                            7:7 /* RWIVF */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_0_BOARD_PGOOD_GPU_RST_DISABLED            0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_0_BOARD_PGOOD_GPU_RST_ENABLED             0x00000001 /* RW--V */

#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_1_BOARD_PGOOD                             0x00000091 /* RW-1R */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_1_BOARD_PGOOD_GPU_RST_HOLD                       0:0 /* RWIVF */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_1_BOARD_PGOOD_GPU_RST_HOLD_DISABLED       0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_1_BOARD_PGOOD_GPU_RST_HOLD_ENABLED        0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_1_BOARD_PGOOD_GC5_MODE                           1:1 /* RWIVF */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_1_BOARD_PGOOD_GC5_MODE_DISABLED           0x00000000 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_1_BOARD_PGOOD_GC5_MODE_ENABLED            0x00000001 /* RWI-V */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_1_BOARD_PGOOD_GC6_FB_EN                          2:2 /* RWIVF */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_1_BOARD_PGOOD_GC6_FB_EN_DISABLED          0x00000000 /* RW--V */
#define LW_EC_INFO_GPIO_OUTPUT_DEFINE_1_BOARD_PGOOD_GC6_FB_EN_ENABLED           0x00000001 /* RWI-V */

/********************************************************************************
 *
 *LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_CFG:  12V PGOOD Function
 *
 * PWR_SRC_PGOOD is an analog measurement of the board's 12V rail. During cold
 * boot, this should rise while SYS_RST is true. At shutdown, this will fall when
 * SYS_RST is true. At all other times, this signal should be asserted.
 *
 *  PGOOD signals are unique in that they can measure real voltage rails. Analog
 *  controls have been added to establish the threshold for PGOOD.
 *
 *  SEL:
 *      GPIO select from the designator pins
 *  INVALID:
 *      GPIO input function is not valid. Don't trust it.
 *  ILWERT:
 *      Treat signal as ilwerted (active low) assert, or active high assert
 *  STATE:
 *      Reflects the signal level after the optional ilwersion and invalid.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_CFG                    0x000000a4 /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_CFG__SIZE_1                     6 /*       */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_CFG_SEL                       4:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_CFG_SEL_GPIO_0         0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_CFG_SEL_GPIO_1         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_CFG_ILWERT                    5:5 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_CFG_ILWERT_NO          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_CFG_ILWERT_YES         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_CFG_ILWALID                   6:6 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_CFG_ILWALID_NO         0x00000000 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_CFG_ILWALID_YES        0x00000001 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_CFG_STATE                     7:7 /* R--VF */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_CFG_STATE_DEASSERTED   0x00000000 /* R---V */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_CFG_STATE_ASSERTED     0x00000001 /* R---V */

/********************************************************************************
 *
 *  TYPE:
 *      Attach an interrupt to this input function. Interrupts are evaluated
 *      after ilwersion and invalid.
 *
 *      NONE - No interrupt
 *      LOW -  Level interrupt. Assert interrupt if signal is de-asserted
 *      HIGH - Level interrupt. Assert interrupt if signal is asserted.
 *      RISING  -  Edge interrupt. Assert interrupt if old state was DEASSERTED
 *                 and new state is ASSERTED
 *      FALLING -  Edge interrupt. Assert interrupt if old state was ASSERTED
 *                 and new state is DEASSERTED
 *      RISEFALL - Edge interrupt. Assert interrupt on any state change
 *      ANLG_GTE - Analog interrupt. Assert interrupt if voltage is greater than
 *                 or equal to the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_LT  - Analog interrupt. Assert interrupt if the voltage is less
 *                 than the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_CHANGE - Analog equivalent to RISEFALL. Assert interrupt on state
 *                    change, where a change in state is relative to a threshold.
 *
 *  STATUS:
 *      PENDING - There is a pending interrupt. Service it.
 *      IDLE - There is no pending interrupt.
 *      CLEAR - Once the interrupt is serviced, clear it by writing 0.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_INTR                    0x000000a5 /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_INTR_TYPE                      3:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_INTR_TYPE_NONE          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_INTR_TYPE_LOW           0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_INTR_TYPE_HIGH          0x00000002 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_INTR_TYPE_RISING        0x00000003 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_INTR_TYPE_FALLING       0x00000004 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_INTR_TYPE_RISEFALL      0x00000005 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_INTR_TYPE_ANLG_GTE      0x00000006 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_INTR_TYPE_ANLG_LT       0x00000007 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_INTR_TYPE_ANLG_CHANGE   0x00000008 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_INTR_STATUS                    7:7 /* RWEVF */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_INTR_STATUS_IDLE        0x00000000 /* RWE-V */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_INTR_STATUS_CLEAR       0x00000001 /* -W--T */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_INTR_STATUS_PENDING     0x00000001 /* R---V */

/******************************************************************************
*   ANLG_0, ANLG_1: Analog threshold for analog interrupt compares
*
*   THRESH_L: Least significant 8 bits of the analog comparator circuit
*   THRESH_H: Most significant 3 bits of the analog comparator circuit
*   UPDATE:   Update the analog comparason circuit. Should change the
*             analog compare circuit to look at this input, and change
*             the threshold detection variable with this value.
*
******************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_ANLG_0                   0x000000a6 /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_ANLG_0_THRESH_L                 7:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_ANLG_0_THRESH_L_NONE     0x00000000 /* RWI-V */

#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_ANLG_1                   0x000000a7 /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_ANLG_1_THRESH_H                 2:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_ANLG_1_THRESH_H_NONE     0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_ANLG_1_UPDATE                   7:7 /* RWEVF */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_ANLG_1_UPDATE_IDLE       0x00000000 /* RWE-V */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_ANLG_1_UPDATE_TRIGGER    0x00000001 /* -W--T */
#define LW_EC_INFO_GPIO_INPUT_PWR_SRC_PGOOD_ANLG_1_UPDATE_PENDING    0x00000001 /* R---V */

/********************************************************************************
 *
 *LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_CFG:  3.3V PGOOD Function
 *
 * 3V3_PGOOD is an analog measurement of the board's 3.3V rail. During cold
 * boot, this should rise while SYS_RST is true. At shutdown, this will fall when
 * SYS_RST is true. At all other times, this signal should be asserted.
 *
 *  PGOOD signals are unique in that they can measure real voltage rails. Analog
 *  controls have been added to establish the threshold for PGOOD.
 *
 *  SEL:
 *      GPIO select from the designator pins
 *  INVALID:
 *      GPIO input function is not valid. Don't trust it.
 *  ILWERT:
 *      Treat signal as ilwerted (active low) assert, or active high assert
 *  STATE:
 *      Reflects the signal level after the optional ilwersion and invalid.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_CFG                    0x000000a8 /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_CFG__SIZE_1                     6 /*       */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_CFG_SEL                       4:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_CFG_SEL_GPIO_0         0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_CFG_SEL_GPIO_1         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_CFG_ILWERT                    5:5 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_CFG_ILWERT_NO          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_CFG_ILWERT_YES         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_CFG_ILWALID                   6:6 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_CFG_ILWALID_NO         0x00000000 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_CFG_ILWALID_YES        0x00000001 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_CFG_STATE                     7:7 /* R--VF */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_CFG_STATE_DEASSERTED   0x00000000 /* R---V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_CFG_STATE_ASSERTED     0x00000001 /* R---V */

/********************************************************************************
 *
 *  TYPE:
 *      Attach an interrupt to this input function. Interrupts are evaluated
 *      after ilwersion and invalid.
 *
 *      NONE - No interrupt
 *      LOW -  Level interrupt. Assert interrupt if signal is de-asserted
 *      HIGH - Level interrupt. Assert interrupt if signal is asserted.
 *      RISING  -  Edge interrupt. Assert interrupt if old state was DEASSERTED
 *                 and new state is ASSERTED
 *      FALLING -  Edge interrupt. Assert interrupt if old state was ASSERTED
 *                 and new state is DEASSERTED
 *      RISEFALL - Edge interrupt. Assert interrupt on any state change
 *      ANLG_GTE - Analog interrupt. Assert interrupt if voltage is greater than
 *                 or equal to the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_LT  - Analog interrupt. Assert interrupt if the voltage is less
 *                 than the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_CHANGE - Analog equivalent to RISEFALL. Assert interrupt on state
 *                    change, where a change in state is relative to a threshold.
 *
 *  STATUS:
 *      PENDING - There is a pending interrupt. Service it.
 *      IDLE - There is no pending interrupt.
 *      CLEAR - Once the interrupt is serviced, clear it by writing 0.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_INTR                    0x000000a9 /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_INTR_TYPE                      3:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_INTR_TYPE_NONE          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_INTR_TYPE_LOW           0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_INTR_TYPE_HIGH          0x00000002 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_INTR_TYPE_RISING        0x00000003 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_INTR_TYPE_FALLING       0x00000004 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_INTR_TYPE_RISEFALL      0x00000005 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_INTR_TYPE_ANLG_GTE      0x00000006 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_INTR_TYPE_ANLG_LT       0x00000007 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_INTR_TYPE_ANLG_CHANGE   0x00000008 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_INTR_STATUS                    7:7 /* RWEVF */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_INTR_STATUS_IDLE        0x00000000 /* RWE-V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_INTR_STATUS_CLEAR       0x00000001 /* -W--T */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_INTR_STATUS_PENDING     0x00000001 /* R---V */

/******************************************************************************
*   ANLG_0, ANLG_1: Analog threshold for analog interrupt compares
*
*   THRESH_L: Least significant 8 bits of the analog comparator circuit
*   THRESH_H: Most significant 3 bits of the analog comparator circuit
*   UPDATE:   Update the analog comparason circuit. Should change the
*             analog compare circuit to look at this input, and change
*             the threshold detection variable with this value.
*
******************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_ANLG_0                   0x000000aa /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_ANLG_0_THRESH_L                 7:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_ANLG_0_THRESH_L_NONE     0x00000000 /* RWI-V */

#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_ANLG_1                   0x000000ab /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_ANLG_1_THRESH_H                 2:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_ANLG_1_THRESH_H_NONE     0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_ANLG_1_UPDATE                   7:7 /* RWEVF */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_ANLG_1_UPDATE_IDLE       0x00000000 /* RWE-V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_ANLG_1_UPDATE_TRIGGER    0x00000001 /* -W--T */
#define LW_EC_INFO_GPIO_INPUT_3V3_PGOOD_ANLG_1_UPDATE_PENDING    0x00000001 /* R---V */

/********************************************************************************
 *
 *LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_CFG:  LWVVDD PGOOD Function
 *
 * LWVDD_PGOOD is a digital measurement of the board's LWVDD PGOOD signal. During
 * cold boot, this should rise while SYS_RST is true. During GC6 and D3 Cold/GOLD,
 * PGOOD should de-assert. At shutdown, PGOOD will de-assert. At all other times,
 * this signal should be asserted.
 *
 *  PGOOD signals are unique in that they can measure real voltage rails. Analog
 *  controls have been added to establish the threshold for PGOOD.
 *
 *  SEL:
 *      GPIO select from the designator pins
 *  INVALID:
 *      GPIO input function is not valid. Don't trust it.
 *  ILWERT:
 *      Treat signal as ilwerted (active low) assert, or active high assert
 *  STATE:
 *      Reflects the signal level after the optional ilwersion and invalid.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_CFG                    0x000000ac /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_CFG__SIZE_1                     6 /*       */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_CFG_SEL                       4:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_CFG_SEL_GPIO_0         0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_CFG_SEL_GPIO_1         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_CFG_ILWERT                    5:5 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_CFG_ILWERT_NO          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_CFG_ILWERT_YES         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_CFG_ILWALID                   6:6 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_CFG_ILWALID_NO         0x00000000 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_CFG_ILWALID_YES        0x00000001 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_CFG_STATE                     7:7 /* R--VF */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_CFG_STATE_DEASSERTED   0x00000000 /* R---V */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_CFG_STATE_ASSERTED     0x00000001 /* R---V */

/********************************************************************************
 *
 *  TYPE:
 *      Attach an interrupt to this input function. Interrupts are evaluated
 *      after ilwersion and invalid.
 *
 *      NONE - No interrupt
 *      LOW -  Level interrupt. Assert interrupt if signal is de-asserted
 *      HIGH - Level interrupt. Assert interrupt if signal is asserted.
 *      RISING  -  Edge interrupt. Assert interrupt if old state was DEASSERTED
 *                 and new state is ASSERTED
 *      FALLING -  Edge interrupt. Assert interrupt if old state was ASSERTED
 *                 and new state is DEASSERTED
 *      RISEFALL - Edge interrupt. Assert interrupt on any state change
 *      ANLG_GTE - Analog interrupt. Assert interrupt if voltage is greater than
 *                 or equal to the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_LT  - Analog interrupt. Assert interrupt if the voltage is less
 *                 than the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_CHANGE - Analog equivalent to RISEFALL. Assert interrupt on state
 *                    change, where a change in state is relative to a threshold.
 *
 *  STATUS:
 *      PENDING - There is a pending interrupt. Service it.
 *      IDLE - There is no pending interrupt.
 *      CLEAR - Once the interrupt is serviced, clear it by writing 0.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_INTR                    0x000000ad /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_INTR_TYPE                      3:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_INTR_TYPE_NONE          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_INTR_TYPE_LOW           0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_INTR_TYPE_HIGH          0x00000002 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_INTR_TYPE_RISING        0x00000003 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_INTR_TYPE_FALLING       0x00000004 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_INTR_TYPE_RISEFALL      0x00000005 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_INTR_TYPE_ANLG_GTE      0x00000006 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_INTR_TYPE_ANLG_LT       0x00000007 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_INTR_TYPE_ANLG_CHANGE   0x00000008 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_INTR_STATUS                    7:7 /* RWEVF */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_INTR_STATUS_IDLE        0x00000000 /* RWE-V */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_INTR_STATUS_CLEAR       0x00000001 /* -W--T */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_INTR_STATUS_PENDING     0x00000001 /* R---V */

/******************************************************************************
*   ANLG_0, ANLG_1: Analog threshold for analog interrupt compares
*
*   THRESH_L: Least significant 8 bits of the analog comparator circuit
*   THRESH_H: Most significant 3 bits of the analog comparator circuit
*   UPDATE:   Update the analog comparason circuit. Should change the
*             analog compare circuit to look at this input, and change
*             the threshold detection variable with this value.
*
******************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_ANLG_0                   0x000000ae /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_ANLG_0_THRESH_L                 7:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_ANLG_0_THRESH_L_NONE     0x00000000 /* RWI-V */

#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_ANLG_1                   0x000000af /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_ANLG_1_THRESH_H                 2:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_ANLG_1_THRESH_H_NONE     0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_ANLG_1_UPDATE                   7:7 /* RWEVF */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_ANLG_1_UPDATE_IDLE       0x00000000 /* RWE-V */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_ANLG_1_UPDATE_TRIGGER    0x00000001 /* -W--T */
#define LW_EC_INFO_GPIO_INPUT_LWVDD_PGOOD_ANLG_1_UPDATE_PENDING    0x00000001 /* R---V */

/********************************************************************************
 *
 *LW_EC_INFO_GPIO_INPUT_1V_PGOOD_CFG:  1V (PEXVDD) PGOOD Function
 *
 * 1V_PGOOD is a digital measurement of the board's 1.05V PGOOD signal. During
 * cold boot, this should rise while SYS_RST is true. During GC6 and D3 Cold/GOLD,
 * PGOOD should de-assert. At shutdown, PGOOD will de-assert. In full GC5, PGOOD
 * will de-assert.  At all other times, this signal should be asserted.
 *
 *  PGOOD signals are unique in that they can measure real voltage rails. Analog
 *  controls have been added to establish the threshold for PGOOD.
 *
 *  SEL:
 *      GPIO select from the designator pins
 *  INVALID:
 *      GPIO input function is not valid. Don't trust it.
 *  ILWERT:
 *      Treat signal as ilwerted (active low) assert, or active high assert
 *  STATE:
 *      Reflects the signal level after the optional ilwersion and invalid.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_CFG                    0x000000b0 /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_CFG__SIZE_1                     6 /*       */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_CFG_SEL                       4:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_CFG_SEL_GPIO_0         0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_CFG_SEL_GPIO_1         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_CFG_ILWERT                    5:5 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_CFG_ILWERT_NO          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_CFG_ILWERT_YES         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_CFG_ILWALID                   6:6 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_CFG_ILWALID_NO         0x00000000 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_CFG_ILWALID_YES        0x00000001 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_CFG_STATE                     7:7 /* R--VF */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_CFG_STATE_DEASSERTED   0x00000000 /* R---V */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_CFG_STATE_ASSERTED     0x00000001 /* R---V */

/********************************************************************************
 *
 *  TYPE:
 *      Attach an interrupt to this input function. Interrupts are evaluated
 *      after ilwersion and invalid.
 *
 *      NONE - No interrupt
 *      LOW -  Level interrupt. Assert interrupt if signal is de-asserted
 *      HIGH - Level interrupt. Assert interrupt if signal is asserted.
 *      RISING  -  Edge interrupt. Assert interrupt if old state was DEASSERTED
 *                 and new state is ASSERTED
 *      FALLING -  Edge interrupt. Assert interrupt if old state was ASSERTED
 *                 and new state is DEASSERTED
 *      RISEFALL - Edge interrupt. Assert interrupt on any state change
 *      ANLG_GTE - Analog interrupt. Assert interrupt if voltage is greater than
 *                 or equal to the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_LT  - Analog interrupt. Assert interrupt if the voltage is less
 *                 than the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_CHANGE - Analog equivalent to RISEFALL. Assert interrupt on state
 *                    change, where a change in state is relative to a threshold.
 *
 *  STATUS:
 *      PENDING - There is a pending interrupt. Service it.
 *      IDLE - There is no pending interrupt.
 *      CLEAR - Once the interrupt is serviced, clear it by writing 0.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_INTR                    0x000000b1 /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_INTR_TYPE                      3:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_INTR_TYPE_NONE          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_INTR_TYPE_LOW           0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_INTR_TYPE_HIGH          0x00000002 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_INTR_TYPE_RISING        0x00000003 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_INTR_TYPE_FALLING       0x00000004 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_INTR_TYPE_RISEFALL      0x00000005 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_INTR_TYPE_ANLG_GTE      0x00000006 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_INTR_TYPE_ANLG_LT       0x00000007 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_INTR_TYPE_ANLG_CHANGE   0x00000008 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_INTR_STATUS                    7:7 /* RWEVF */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_INTR_STATUS_IDLE        0x00000000 /* RWE-V */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_INTR_STATUS_CLEAR       0x00000001 /* -W--T */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_INTR_STATUS_PENDING     0x00000001 /* R---V */

/******************************************************************************
*   ANLG_0, ANLG_1: Analog threshold for analog interrupt compares
*
*   THRESH_L: Least significant 8 bits of the analog comparator circuit
*   THRESH_H: Most significant 3 bits of the analog comparator circuit
*   UPDATE:   Update the analog comparason circuit. Should change the
*             analog compare circuit to look at this input, and change
*             the threshold detection variable with this value.
*
******************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_ANLG_0                   0x000000b2 /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_ANLG_0_THRESH_L                 7:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_ANLG_0_THRESH_L_NONE     0x00000000 /* RWI-V */

#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_ANLG_1                   0x000000b3 /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_ANLG_1_THRESH_H                 2:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_ANLG_1_THRESH_H_NONE     0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_ANLG_1_UPDATE                   7:7 /* RWEVF */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_ANLG_1_UPDATE_IDLE       0x00000000 /* RWE-V */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_ANLG_1_UPDATE_TRIGGER    0x00000001 /* -W--T */
#define LW_EC_INFO_GPIO_INPUT_1V_PGOOD_ANLG_1_UPDATE_PENDING    0x00000001 /* R---V */

/********************************************************************************
 *
 *LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_CFG:  GPU_FBIO (FBVDDQ_GPU) PGOOD Function
 *
 * GPU_FBIO_PGOOD is a digital measurement of the board's GPU_FBIO PGOOD signal.0
 * During cold boot, this should rise while SYS_RST is true. During GC6-SP and
 * D3 Cold/GOLD, PGOOD should de-assert. At shutdown, PGOOD will de-assert. At all
 * other times (including GC6), this signal should be asserted.
 *
 *  PGOOD signals are unique in that they can measure real voltage rails. Analog
 *  controls have been added to establish the threshold for PGOOD.
 *
 *  SEL:
 *      GPIO select from the designator pins
 *  INVALID:
 *      GPIO input function is not valid. Don't trust it.
 *  ILWERT:
 *      Treat signal as ilwerted (active low) assert, or active high assert
 *  STATE:
 *      Reflects the signal level after the optional ilwersion and invalid.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_CFG                    0x000000b4 /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_CFG__SIZE_1                     6 /*       */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_CFG_SEL                       4:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_CFG_SEL_GPIO_0         0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_CFG_SEL_GPIO_1         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_CFG_ILWERT                    5:5 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_CFG_ILWERT_NO          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_CFG_ILWERT_YES         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_CFG_ILWALID                   6:6 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_CFG_ILWALID_NO         0x00000000 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_CFG_ILWALID_YES        0x00000001 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_CFG_STATE                     7:7 /* R--VF */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_CFG_STATE_DEASSERTED   0x00000000 /* R---V */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_CFG_STATE_ASSERTED     0x00000001 /* R---V */

/********************************************************************************
 *
 *  TYPE:
 *      Attach an interrupt to this input function. Interrupts are evaluated
 *      after ilwersion and invalid.
 *
 *      NONE - No interrupt
 *      LOW -  Level interrupt. Assert interrupt if signal is de-asserted
 *      HIGH - Level interrupt. Assert interrupt if signal is asserted.
 *      RISING  -  Edge interrupt. Assert interrupt if old state was DEASSERTED
 *                 and new state is ASSERTED
 *      FALLING -  Edge interrupt. Assert interrupt if old state was ASSERTED
 *                 and new state is DEASSERTED
 *      RISEFALL - Edge interrupt. Assert interrupt on any state change
 *      ANLG_GTE - Analog interrupt. Assert interrupt if voltage is greater than
 *                 or equal to the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_LT  - Analog interrupt. Assert interrupt if the voltage is less
 *                 than the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_CHANGE - Analog equivalent to RISEFALL. Assert interrupt on state
 *                    change, where a change in state is relative to a threshold.
 *
 *  STATUS:
 *      PENDING - There is a pending interrupt. Service it.
 *      IDLE - There is no pending interrupt.
 *      CLEAR - Once the interrupt is serviced, clear it by writing 0.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_INTR                    0x000000b5 /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_INTR_TYPE                      3:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_INTR_TYPE_NONE          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_INTR_TYPE_LOW           0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_INTR_TYPE_HIGH          0x00000002 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_INTR_TYPE_RISING        0x00000003 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_INTR_TYPE_FALLING       0x00000004 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_INTR_TYPE_RISEFALL      0x00000005 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_INTR_TYPE_ANLG_GTE      0x00000006 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_INTR_TYPE_ANLG_LT       0x00000007 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_INTR_TYPE_ANLG_CHANGE   0x00000008 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_INTR_STATUS                    7:7 /* RWEVF */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_INTR_STATUS_IDLE        0x00000000 /* RWE-V */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_INTR_STATUS_CLEAR       0x00000001 /* -W--T */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_INTR_STATUS_PENDING     0x00000001 /* R---V */

/******************************************************************************
*   ANLG_0, ANLG_1: Analog threshold for analog interrupt compares
*
*   THRESH_L: Least significant 8 bits of the analog comparator circuit
*   THRESH_H: Most significant 3 bits of the analog comparator circuit
*   UPDATE:   Update the analog comparason circuit. Should change the
*             analog compare circuit to look at this input, and change
*             the threshold detection variable with this value.
*
******************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_ANLG_0                   0x000000b6 /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_ANLG_0_THRESH_L                 7:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_ANLG_0_THRESH_L_NONE     0x00000000 /* RWI-V */

#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_ANLG_1                   0x000000b7 /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_ANLG_1_THRESH_H                 2:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_ANLG_1_THRESH_H_NONE     0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_ANLG_1_UPDATE                   7:7 /* RWEVF */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_ANLG_1_UPDATE_IDLE       0x00000000 /* RWE-V */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_ANLG_1_UPDATE_TRIGGER    0x00000001 /* -W--T */
#define LW_EC_INFO_GPIO_INPUT_GPU_FBIO_PGOOD_ANLG_1_UPDATE_PENDING    0x00000001 /* R---V */

/********************************************************************************
 *
 *LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_CFG:  VRAM TOTAL (FBVDD/Q_MEM) PGOOD0
                                               Function
 *
 * VRAM_TOTAL_PGOOD is a digital measurement of the board's VRAM_TOTAL PGOOD signal.
 * During cold boot, this should rise while SYS_RST is true. During D3 Cold/GOLD,
 * PGOOD should de-assert. At shutdown, PGOOD will de-assert. At all
 * other times (including GC6 and GC6-SP), this signal should be asserted.
 *
 *  PGOOD signals are unique in that they can measure real voltage rails. Analog
 *  controls have been added to establish the threshold for PGOOD.
 *
 *  SEL:
 *      GPIO select from the designator pins
 *  INVALID:
 *      GPIO input function is not valid. Don't trust it.
 *  ILWERT:
 *      Treat signal as ilwerted (active low) assert, or active high assert
 *  STATE:
 *      Reflects the signal level after the optional ilwersion and invalid.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_CFG                    0x000000b8 /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_CFG__SIZE_1                     6 /*       */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_CFG_SEL                       4:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_CFG_SEL_GPIO_0         0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_CFG_SEL_GPIO_1         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_CFG_ILWERT                    5:5 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_CFG_ILWERT_NO          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_CFG_ILWERT_YES         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_CFG_ILWALID                   6:6 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_CFG_ILWALID_NO         0x00000000 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_CFG_ILWALID_YES        0x00000001 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_CFG_STATE                     7:7 /* R--VF */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_CFG_STATE_DEASSERTED   0x00000000 /* R---V */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_CFG_STATE_ASSERTED     0x00000001 /* R---V */

/********************************************************************************
 *
 *  TYPE:
 *      Attach an interrupt to this input function. Interrupts are evaluated
 *      after ilwersion and invalid.
 *
 *      NONE - No interrupt
 *      LOW -  Level interrupt. Assert interrupt if signal is de-asserted
 *      HIGH - Level interrupt. Assert interrupt if signal is asserted.
 *      RISING  -  Edge interrupt. Assert interrupt if old state was DEASSERTED
 *                 and new state is ASSERTED
 *      FALLING -  Edge interrupt. Assert interrupt if old state was ASSERTED
 *                 and new state is DEASSERTED
 *      RISEFALL - Edge interrupt. Assert interrupt on any state change
 *      ANLG_GTE - Analog interrupt. Assert interrupt if voltage is greater than
 *                 or equal to the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_LT  - Analog interrupt. Assert interrupt if the voltage is less
 *                 than the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_CHANGE - Analog equivalent to RISEFALL. Assert interrupt on state
 *                    change, where a change in state is relative to a threshold.
 *
 *  STATUS:
 *      PENDING - There is a pending interrupt. Service it.
 *      IDLE - There is no pending interrupt.
 *      CLEAR - Once the interrupt is serviced, clear it by writing 0.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_INTR                    0x000000b9 /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_INTR_TYPE                      3:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_INTR_TYPE_NONE          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_INTR_TYPE_LOW           0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_INTR_TYPE_HIGH          0x00000002 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_INTR_TYPE_RISING        0x00000003 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_INTR_TYPE_FALLING       0x00000004 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_INTR_TYPE_RISEFALL      0x00000005 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_INTR_TYPE_ANLG_GTE      0x00000006 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_INTR_TYPE_ANLG_LT       0x00000007 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_INTR_TYPE_ANLG_CHANGE   0x00000008 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_INTR_STATUS                    7:7 /* RWEVF */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_INTR_STATUS_IDLE        0x00000000 /* RWE-V */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_INTR_STATUS_CLEAR       0x00000001 /* -W--T */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_INTR_STATUS_PENDING     0x00000001 /* R---V */

/******************************************************************************
*   ANLG_0, ANLG_1: Analog threshold for analog interrupt compares
*
*   THRESH_L: Least significant 8 bits of the analog comparator circuit
*   THRESH_H: Most significant 3 bits of the analog comparator circuit
*   UPDATE:   Update the analog comparason circuit. Should change the
*             analog compare circuit to look at this input, and change
*             the threshold detection variable with this value.
*
******************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_ANLG_0                   0x000000ba /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_ANLG_0_THRESH_L                 7:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_ANLG_0_THRESH_L_NONE     0x00000000 /* RWI-V */

#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_ANLG_1                   0x000000bb /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_ANLG_1_THRESH_H                 2:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_ANLG_1_THRESH_H_NONE     0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_ANLG_1_UPDATE                   7:7 /* RWEVF */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_ANLG_1_UPDATE_IDLE       0x00000000 /* RWE-V */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_ANLG_1_UPDATE_TRIGGER    0x00000001 /* -W--T */
#define LW_EC_INFO_GPIO_INPUT_VRAM_TOTAL_PGOOD_ANLG_1_UPDATE_PENDING    0x00000001 /* R---V */

/********************************************************************************
 *
 *LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_CFG:  3V3_PIC PGOOD  Function
 *
 * 3V3_PIC_PGOOD is a digital measurement of the board's 3V3_PIC_PGOOD signal.
 * The regulator that powers 3V3_PIC sources both 3V3_MAIN and 3V3_AON.
 * During cold boot, this should rise while SYS_RST is true. At shutdown and S3,
 * PGOOD will de-assert. At all other times (including GC6, GC6-SP, and D3_COLD),
 * this signal should be asserted.
 *
 *  PGOOD signals are unique in that they can measure real voltage rails. Analog
 *  controls have been added to establish the threshold for PGOOD.
 *
 *  SEL:
 *      GPIO select from the designator pins
 *  INVALID:
 *      GPIO input function is not valid. Don't trust it.
 *  ILWERT:
 *      Treat signal as ilwerted (active low) assert, or active high assert
 *  STATE:
 *      Reflects the signal level after the optional ilwersion and invalid.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_CFG                    0x000000bc /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_CFG__SIZE_1                     6 /*       */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_CFG_SEL                       4:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_CFG_SEL_GPIO_0         0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_CFG_SEL_GPIO_1         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_CFG_ILWERT                    5:5 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_CFG_ILWERT_NO          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_CFG_ILWERT_YES         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_CFG_ILWALID                   6:6 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_CFG_ILWALID_NO         0x00000000 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_CFG_ILWALID_YES        0x00000001 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_CFG_STATE                     7:7 /* R--VF */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_CFG_STATE_DEASSERTED   0x00000000 /* R---V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_CFG_STATE_ASSERTED     0x00000001 /* R---V */

/********************************************************************************
 *
 *  TYPE:
 *      Attach an interrupt to this input function. Interrupts are evaluated
 *      after ilwersion and invalid.
 *
 *      NONE - No interrupt
 *      LOW -  Level interrupt. Assert interrupt if signal is de-asserted
 *      HIGH - Level interrupt. Assert interrupt if signal is asserted.
 *      RISING  -  Edge interrupt. Assert interrupt if old state was DEASSERTED
 *                 and new state is ASSERTED
 *      FALLING -  Edge interrupt. Assert interrupt if old state was ASSERTED
 *                 and new state is DEASSERTED
 *      RISEFALL - Edge interrupt. Assert interrupt on any state change
 *      ANLG_GTE - Analog interrupt. Assert interrupt if voltage is greater than
 *                 or equal to the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_LT  - Analog interrupt. Assert interrupt if the voltage is less
 *                 than the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_CHANGE - Analog equivalent to RISEFALL. Assert interrupt on state
 *                    change, where a change in state is relative to a threshold.
 *
 *  STATUS:
 *      PENDING - There is a pending interrupt. Service it.
 *      IDLE - There is no pending interrupt.
 *      CLEAR - Once the interrupt is serviced, clear it by writing 0.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_INTR                    0x000000bd /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_INTR_TYPE                      3:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_INTR_TYPE_NONE          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_INTR_TYPE_LOW           0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_INTR_TYPE_HIGH          0x00000002 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_INTR_TYPE_RISING        0x00000003 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_INTR_TYPE_FALLING       0x00000004 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_INTR_TYPE_RISEFALL      0x00000005 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_INTR_TYPE_ANLG_GTE      0x00000006 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_INTR_TYPE_ANLG_LT       0x00000007 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_INTR_TYPE_ANLG_CHANGE   0x00000008 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_INTR_STATUS                    7:7 /* RWEVF */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_INTR_STATUS_IDLE        0x00000000 /* RWE-V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_INTR_STATUS_CLEAR       0x00000001 /* -W--T */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_INTR_STATUS_PENDING     0x00000001 /* R---V */

/******************************************************************************
*   ANLG_0, ANLG_1: Analog threshold for analog interrupt compares
*
*   THRESH_L: Least significant 8 bits of the analog comparator circuit
*   THRESH_H: Most significant 3 bits of the analog comparator circuit
*   UPDATE:   Update the analog comparason circuit. Should change the
*             analog compare circuit to look at this input, and change
*             the threshold detection variable with this value.
*
******************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_ANLG_0                   0x000000be /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_ANLG_0_THRESH_L                 7:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_ANLG_0_THRESH_L_NONE     0x00000000 /* RWI-V */

#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_ANLG_1                   0x000000bf /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_ANLG_1_THRESH_H                 2:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_ANLG_1_THRESH_H_NONE     0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_ANLG_1_UPDATE                   7:7 /* RWEVF */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_ANLG_1_UPDATE_IDLE       0x00000000 /* RWE-V */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_ANLG_1_UPDATE_TRIGGER    0x00000001 /* -W--T */
#define LW_EC_INFO_GPIO_INPUT_3V3_PIC_PGOOD_ANLG_1_UPDATE_PENDING    0x00000001 /* R---V */

/********************************************************************************
 *
 *LW_EC_INFO_GPIO_INPUT_DGPU_EN_CFG:  DGPU_EN function
 *
 * DGPU_EN: Signal Driven From the embedded controller. It is used for
 *           powerdown and power up behavior. Here are the relevant defines
 *
 *         IDLE_UNINIT = !DGPU_EN && !DGPU_SEL && !PEX_RST
 *         ---> This is the default state after cold boot. Before the platform
 *              can trigger GOLD or GC6 powerdowns, the conditions for IDLE_INIT
 *              must be met.
 *
 *         SYSTEM_RESET = !DGPU_EN && !DGPU_SEL && PEX_RST
 *         ---> If a SYSTEM_RESET oclwrs, the master FSM should gracefully go to
 *              the INIT state.
 *
 *              If the FW is in a powered down state:
 *                  *If SYSTEM_RESET is accompanied by a platform powerdown,
 *                  go directly to INIT.
 *                  *If just a SYSTEM_RESET, power up normally
 *
 *         If the Master FSM is in FULL_PWR.
 *         --->IDLE_INIT = DGPU_EN && DGPU_SEL
 *         --->POWERDOWN_PREP = !DGPU_EN && DGPU_SEL
 *                  *FB_CLAMP handshake done means GC6K entry
 *                  *PEX_RST assertion means GOLD entry
 *         If the Master FSM is in GC6 or GC6M.
 *         --->GC6_PWRUP_SR_UPDATE = DGPU_EN && !DGPU_SEL
 *                  *Platform can de-assert DGPU_SEL after 2ms to go to IDLE_INIT
 *         --->GC6_PWRUP_FULL      = DGPU_EN && DGPU_SEL
 *         If the Master FSM is in D3_COLD
 *         --->FULL_PWRUP          = DGPU_EN && DGPU_SEL
 *
 *  CFG_SEL:
 *      GPIO select from the designator pins
 *  CFG_ILWALID:
 *      GPIO input function is not valid. Don't trust it.
 *  CFG_ILWERT:
 *      Treat signal as ilwerted (active low) assert, or active high assert
 *  CFG_STATE:
 *      Reflects the signal level after the optional ilwersion and invalid.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_DGPU_EN_CFG                    0x000000d0 /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_DGPU_EN_CFG_SEL                       4:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_DGPU_EN_CFG_SEL_GPIO_0         0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_EN_CFG_SEL_GPIO_1         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_EN_CFG_ILWERT                    5:5 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_DGPU_EN_CFG_ILWERT_NO          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_EN_CFG_ILWERT_YES         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_EN_CFG_ILWALID                   6:6 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_DGPU_EN_CFG_ILWALID_NO         0x00000000 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_EN_CFG_ILWALID_YES        0x00000001 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_EN_CFG_STATE                     7:7 /* R--VF */
#define LW_EC_INFO_GPIO_INPUT_DGPU_EN_CFG_STATE_DEASSERTED   0x00000000 /* R---V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_EN_CFG_STATE_ASSERTED     0x00000001 /* R---V */

/********************************************************************************
 *
 *  TYPE:
 *      Attach an interrupt to this input function. Interrupts are evaluated
 *      after ilwersion and invalid.
 *
 *      NONE - No interrupt
 *      LOW -  Level interrupt. Assert interrupt if signal is de-asserted
 *      HIGH - Level interrupt. Assert interrupt if signal is asserted.
 *      RISING  -  Edge interrupt. Assert interrupt if old state was DEASSERTED
 *                 and new state is ASSERTED
 *      FALLING -  Edge interrupt. Assert interrupt if old state was ASSERTED
 *                 and new state is DEASSERTED
 *      RISEFALL - Edge interrupt. Assert interrupt on any state change
 *      ANLG_GTE - Analog interrupt. Assert interrupt if voltage is greater than
 *                 or equal to the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_LT  - Analog interrupt. Assert interrupt if the voltage is less
 *                 than the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_CHANGE - Analog equivalent to RISEFALL. Assert interrupt on state
 *                    change, where a change in state is relative to a threshold.
 *  STATUS:
 *      PENDING - There is a pending interrupt. Service it.
 *      IDLE - There is no pending interrupt.
 *      CLEAR - Once the interrupt is serviced, clear it by writing 0.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_DGPU_EN_INTR                    0x000000d1 /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_DGPU_EN_INTR_TYPE                      3:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_DGPU_EN_INTR_TYPE_NONE          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_EN_INTR_TYPE_LOW           0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_EN_INTR_TYPE_HIGH          0x00000002 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_EN_INTR_TYPE_RISING        0x00000003 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_EN_INTR_TYPE_FALLING       0x00000004 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_EN_INTR_TYPE_RISEFALL      0x00000005 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_EN_INTR_TYPE_ANLG_GTE      0x00000006 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_EN_INTR_TYPE_ANLG_LT       0x00000007 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_EN_INTR_TYPE_ANLG_CHANGE   0x00000008 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_EN_INTR_STATUS                    7:7 /* RWEVF */
#define LW_EC_INFO_GPIO_INPUT_DGPU_EN_INTR_STATUS_IDLE        0x00000000 /* RWE-V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_EN_INTR_STATUS_CLEAR       0x00000001 /* -W--T */
#define LW_EC_INFO_GPIO_INPUT_DGPU_EN_INTR_STATUS_PENDING     0x00000001 /* R---V */

/********************************************************************************
 *
 *LW_EC_INFO_GPIO_INPUT_DGPU_SEL_CFG:  DGPU_SEL function
 *
 * DGPU_SEL: Signal Driven From the embedded controller. It is used for
 *           powerdown and power up behavior. Here are the relevant defines
 *
 *         IDLE_UNINIT = !DGPU_EN && !DGPU_SEL && !PEX_RST
 *         ---> This is the default state after cold boot. Before the platform
 *              can trigger GOLD or GC6 powerdowns, the conditions for IDLE_INIT
 *              must be met.
 *
 *         SYSTEM_RESET = !DGPU_EN && !DGPU_SEL && PEX_RST
 *         ---> If a SYSTEM_RESET oclwrs, the master FSM should gracefully go to
 *              the INIT state.
 *
 *              If the FW is in a powered down state:
 *                  *If SYSTEM_RESET is accompanied by a platform powerdown,
 *                  go directly to INIT.
 *                  *If just a SYSTEM_RESET, power up normally
 *
 *         If the Master FSM is in FULL_PWR.
 *         --->IDLE_INIT = DGPU_EN && DGPU_SEL
 *         --->POWERDOWN_PREP = !DGPU_EN && DGPU_SEL
 *                  *FB_CLAMP handshake done means GC6K entry
 *                  *PEX_RST assertion means GOLD entry
 *         If the Master FSM is in GC6 or GC6M.
 *         --->GC6_PWRUP_SR_UPDATE = DGPU_EN && !DGPU_SEL
 *                  *Platform can de-assert DGPU_SEL after 2ms to go to IDLE_INIT
 *         --->GC6_PWRUP_FULL      = DGPU_EN && DGPU_SEL
 *         If the Master FSM is in D3_COLD
 *         --->FULL_PWRUP          = DGPU_EN && DGPU_SEL
 *
 *  SEL:
 *      GPIO select from the designator pins
 *  INVALID:
 *      GPIO input function is not valid. Don't trust it.
 *  ILWERT:
 *      Treat signal as ilwerted (active low) assert, or active high assert
 *  STATE:
 *      Reflects the signal level after the optional ilwersion and invalid.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_DGPU_SEL_CFG                    0x000000d2 /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_DGPU_SEL_CFG_SEL                       4:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_DGPU_SEL_CFG_SEL_GPIO_0         0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_SEL_CFG_SEL_GPIO_1         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_SEL_CFG_ILWERT                    5:5 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_DGPU_SEL_CFG_ILWERT_NO          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_SEL_CFG_ILWERT_YES         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_SEL_CFG_ILWALID                   6:6 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_DGPU_SEL_CFG_ILWALID_NO         0x00000000 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_SEL_CFG_ILWALID_YES        0x00000001 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_SEL_CFG_STATE                     7:7 /* R--VF */
#define LW_EC_INFO_GPIO_INPUT_DGPU_SEL_CFG_STATE_DEASSERTED   0x00000000 /* R---V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_SEL_CFG_STATE_ASSERTED     0x00000001 /* R---V */

/********************************************************************************
 *
 *  TYPE:
 *      Attach an interrupt to this input function. Interrupts are evaluated
 *      after ilwersion and invalid.
 *
 *      NONE - No interrupt
 *      LOW -  Level interrupt. Assert interrupt if signal is de-asserted
 *      HIGH - Level interrupt. Assert interrupt if signal is asserted.
 *      RISING  -  Edge interrupt. Assert interrupt if old state was DEASSERTED
 *                 and new state is ASSERTED
 *      FALLING -  Edge interrupt. Assert interrupt if old state was ASSERTED
 *                 and new state is DEASSERTED
 *      RISEFALL - Edge interrupt. Assert interrupt on any state change
 *      ANLG_GTE - Analog interrupt. Assert interrupt if voltage is greater than
 *                 or equal to the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_LT  - Analog interrupt. Assert interrupt if the voltage is less
 *                 than the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_CHANGE - Analog equivalent to RISEFALL. Assert interrupt on state
 *                    change, where a change in state is relative to a threshold.
 *  STATUS:
 *      PENDING - There is a pending interrupt. Service it.
 *      IDLE - There is no pending interrupt.
 *      CLEAR - Once the interrupt is serviced, clear it by writing 0.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_DGPU_SEL_INTR                    0x000000d3 /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_DGPU_SEL_INTR_TYPE                      3:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_DGPU_SEL_INTR_TYPE_NONE          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_SEL_INTR_TYPE_LOW           0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_SEL_INTR_TYPE_HIGH          0x00000002 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_SEL_INTR_TYPE_RISING        0x00000003 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_SEL_INTR_TYPE_FALLING       0x00000004 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_SEL_INTR_TYPE_RISEFALL      0x00000005 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_SEL_INTR_TYPE_ANLG_GTE      0x00000006 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_SEL_INTR_TYPE_ANLG_LT       0x00000007 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_SEL_INTR_TYPE_ANLG_CHANGE   0x00000008 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_SEL_INTR_STATUS                    7:7 /* RWEVF */
#define LW_EC_INFO_GPIO_INPUT_DGPU_SEL_INTR_STATUS_IDLE        0x00000000 /* RWE-V */
#define LW_EC_INFO_GPIO_INPUT_DGPU_SEL_INTR_STATUS_CLEAR       0x00000001 /* -W--T */
#define LW_EC_INFO_GPIO_INPUT_DGPU_SEL_INTR_STATUS_PENDING     0x00000001 /* R---V */

/********************************************************************************
 *
 *LW_EC_INFO_GPIO_INPUT_GPU_EVENT_CFG:  GPU_EVENT# function
 *
 * GPU_EVENT# is a signal defined in the GC6M spec. It is an open-drain, ilwerted
 * bi-directional signal.
 *
 * Expected behavior:
 *      GC6M-SDR: This signal should be hooked up to an interrupt.
 *                When GPU_EVENT# is asserted from the GPU, the interrupt
 *                should trigger the wakeup sequence to commence.
 *
 *      GC6M-ENR: This signal should be hooked up to an interrupt.
 *                When GPU_EVENT# is asserted from the GPU, the interrupt
 *                should trigger the Master FSM to toggle the state of
 *                PWR_SEQ_GPU_RST_PIC.
 *
 *      GC6M-SDR (self-driven reset) is the POR
 *      GC6M-ENR (ec-negotiated reset) is not lwrrently supported
 *
 *
 *  SEL:
 *      GPIO select from the designator pins
 *  INVALID:
 *      GPIO input function is not valid. Don't trust it.
 *  ILWERT:
 *      Treat signal as ilwerted (active low) assert, or active high assert
 *  STATE:
 *      Reflects the signal level after the optional ilwersion and invalid.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_GPU_EVENT_CFG                    0x000000d4 /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_GPU_EVENT_CFG_SEL                       4:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_GPU_EVENT_CFG_SEL_GPIO_0         0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_GPU_EVENT_CFG_SEL_GPIO_1         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_EVENT_CFG_ILWERT                    5:5 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_GPU_EVENT_CFG_ILWERT_NO          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_GPU_EVENT_CFG_ILWERT_YES         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_EVENT_CFG_ILWALID                   6:6 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_GPU_EVENT_CFG_ILWALID_NO         0x00000000 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_EVENT_CFG_ILWALID_YES        0x00000001 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_GPU_EVENT_CFG_STATE                     7:7 /* R--VF */
#define LW_EC_INFO_GPIO_INPUT_GPU_EVENT_CFG_STATE_DEASSERTED   0x00000000 /* R---V */
#define LW_EC_INFO_GPIO_INPUT_GPU_EVENT_CFG_STATE_ASSERTED     0x00000001 /* R---V */

/********************************************************************************
 *
 *  TYPE:
 *      Attach an interrupt to this input function. Interrupts are evaluated
 *      after ilwersion and invalid.
 *
 *      NONE - No interrupt
 *      LOW -  Level interrupt. Assert interrupt if signal is de-asserted
 *      HIGH - Level interrupt. Assert interrupt if signal is asserted.
 *      RISING  -  Edge interrupt. Assert interrupt if old state was DEASSERTED
 *                 and new state is ASSERTED
 *      FALLING -  Edge interrupt. Assert interrupt if old state was ASSERTED
 *                 and new state is DEASSERTED
 *      RISEFALL - Edge interrupt. Assert interrupt on any state change
 *      ANLG_GTE - Analog interrupt. Assert interrupt if voltage is greater than
 *                 or equal to the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_LT  - Analog interrupt. Assert interrupt if the voltage is less
 *                 than the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_CHANGE - Analog equivalent to RISEFALL. Assert interrupt on state
 *                    change, where a change in state is relative to a threshold.
 *  STATUS:
 *      PENDING - There is a pending interrupt. Service it.
 *      IDLE - There is no pending interrupt.
 *      CLEAR - Once the interrupt is serviced, clear it by writing 0.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_GPU_EVENT_INTR                    0x000000d5 /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_GPU_EVENT_INTR_TYPE                      3:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_GPU_EVENT_INTR_TYPE_NONE          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_GPU_EVENT_INTR_TYPE_LOW           0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_EVENT_INTR_TYPE_HIGH          0x00000002 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_EVENT_INTR_TYPE_RISING        0x00000003 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_EVENT_INTR_TYPE_FALLING       0x00000004 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_EVENT_INTR_TYPE_RISEFALL      0x00000005 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_EVENT_INTR_TYPE_ANLG_GTE      0x00000006 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_EVENT_INTR_TYPE_ANLG_LT       0x00000007 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_EVENT_INTR_TYPE_ANLG_CHANGE   0x00000008 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_EVENT_INTR_STATUS                    7:7 /* RWEVF */
#define LW_EC_INFO_GPIO_INPUT_GPU_EVENT_INTR_STATUS_IDLE        0x00000000 /* RWE-V */
#define LW_EC_INFO_GPIO_INPUT_GPU_EVENT_INTR_STATUS_CLEAR       0x00000001 /* -W--T */
#define LW_EC_INFO_GPIO_INPUT_GPU_EVENT_INTR_STATUS_PENDING     0x00000001 /* R---V */

/********************************************************************************
 *
 *LW_EC_INFO_GPIO_INPUT_FBCLAMP_TGL_REQ_CFG:  FBCLAMP_TGL_REQ# function
 *
 * FBCLAMP_TGL_REQ# is a signal defined in the GC6K spec. It is an open-drain,
 * ilwerted signal.
 *
 * Expected behavior:
 *      The GPU asserts FBCLAMP_TGL_REQ# when it wants to change the state of
 *      FB_CLAMP, which is an output control from the EC. Afterwards, the GPU
 *      de-asserts FBCLAMP_TGL_REQ, to complete the handshake.
 *
 *      On FBCLAMP_TGL_REQ# Assertion:
 *          FB_CLAMP_next = !FB_CLAMP
 *          fbc_rdy_idle = 0
 *
 *      On FBCLAMP_TGL_REQ# De-assertion:
 *          FB_CLAMP_next = FB_CLAMP (no change)
 *          fbc_rdy_idle = FB_CLAMP
 *
 *      fbc_rdy_idle is used to determine, along with DGPU_SEL and DGPU_EN, if the
 *      EC is allowed to power down into GC6.
 *
 *
 *  SEL:
 *      GPIO select from the designator pins
 *  INVALID:
 *      GPIO input function is not valid. Don't trust it.
 *  ILWERT:
 *      Treat signal as ilwerted (active low) assert, or active high assert
 *  STATE:
 *      Reflects the signal level after the optional ilwersion and invalid.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_FBCLAMP_TGL_REQ_CFG                    0x000000d6 /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_FBCLAMP_TGL_REQ_CFG_SEL                       4:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_FBCLAMP_TGL_REQ_CFG_SEL_GPIO_0         0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_FBCLAMP_TGL_REQ_CFG_SEL_GPIO_1         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_FBCLAMP_TGL_REQ_CFG_ILWERT                    5:5 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_FBCLAMP_TGL_REQ_CFG_ILWERT_NO          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_FBCLAMP_TGL_REQ_CFG_ILWERT_YES         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_FBCLAMP_TGL_REQ_CFG_ILWALID                   6:6 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_FBCLAMP_TGL_REQ_CFG_ILWALID_NO         0x00000000 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_FBCLAMP_TGL_REQ_CFG_ILWALID_YES        0x00000001 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_FBCLAMP_TGL_REQ_CFG_STATE                     7:7 /* R--VF */
#define LW_EC_INFO_GPIO_INPUT_FBCLAMP_TGL_REQ_CFG_STATE_DEASSERTED   0x00000000 /* R---V */
#define LW_EC_INFO_GPIO_INPUT_FBCLAMP_TGL_REQ_CFG_STATE_ASSERTED     0x00000001 /* R---V */

/********************************************************************************
 *
 *  TYPE:
 *      Attach an interrupt to this input function. Interrupts are evaluated
 *      after ilwersion and invalid.
 *
 *      NONE - No interrupt
 *      LOW -  Level interrupt. Assert interrupt if signal is de-asserted
 *      HIGH - Level interrupt. Assert interrupt if signal is asserted.
 *      RISING  -  Edge interrupt. Assert interrupt if old state was DEASSERTED
 *                 and new state is ASSERTED
 *      FALLING -  Edge interrupt. Assert interrupt if old state was ASSERTED
 *                 and new state is DEASSERTED
 *      RISEFALL - Edge interrupt. Assert interrupt on any state change
 *      ANLG_GTE - Analog interrupt. Assert interrupt if voltage is greater than
 *                 or equal to the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_LT  - Analog interrupt. Assert interrupt if the voltage is less
 *                 than the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_CHANGE - Analog equivalent to RISEFALL. Assert interrupt on state
 *                    change, where a change in state is relative to a threshold.
 *  STATUS:
 *      PENDING - There is a pending interrupt. Service it.
 *      IDLE - There is no pending interrupt.
 *      CLEAR - Once the interrupt is serviced, clear it by writing 0.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_FBCLAMP_TGL_REQ_INTR                    0x000000d7 /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_FBCLAMP_TGL_REQ_INTR_TYPE                      3:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_FBCLAMP_TGL_REQ_INTR_TYPE_NONE          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_FBCLAMP_TGL_REQ_INTR_TYPE_LOW           0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_FBCLAMP_TGL_REQ_INTR_TYPE_HIGH          0x00000002 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_FBCLAMP_TGL_REQ_INTR_TYPE_RISING        0x00000003 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_FBCLAMP_TGL_REQ_INTR_TYPE_FALLING       0x00000004 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_FBCLAMP_TGL_REQ_INTR_TYPE_RISEFALL      0x00000005 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_FBCLAMP_TGL_REQ_INTR_TYPE_ANLG_GTE      0x00000006 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_FBCLAMP_TGL_REQ_INTR_TYPE_ANLG_LT       0x00000007 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_FBCLAMP_TGL_REQ_INTR_TYPE_ANLG_CHANGE   0x00000008 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_FBCLAMP_TGL_REQ_INTR_STATUS                    7:7 /* RWEVF */
#define LW_EC_INFO_GPIO_INPUT_FBCLAMP_TGL_REQ_INTR_STATUS_IDLE        0x00000000 /* RWE-V */
#define LW_EC_INFO_GPIO_INPUT_FBCLAMP_TGL_REQ_INTR_STATUS_CLEAR       0x00000001 /* -W--T */
#define LW_EC_INFO_GPIO_INPUT_FBCLAMP_TGL_REQ_INTR_STATUS_PENDING     0x00000001 /* R---V */

/********************************************************************************
 *
 *LW_EC_INFO_GPIO_INPUT_PEX_RST_CFG:  PEX_RST# function
 *
 * PEX_RST# is the global reset signal sent to the GPU via the PCIE connector. It's
 * an active low signal.
 *
 *         IDLE_UNINIT = !DGPU_EN && !DGPU_SEL && !PEX_RST
 *         ---> This is the default state after cold boot. Before the platform
 *              can trigger GOLD or GC6 powerdowns, the conditions for IDLE_INIT
 *              must be met.
 *
 *         SYSTEM_RESET = !DGPU_EN && !DGPU_SEL && PEX_RST
 *         ---> If a SYSTEM_RESET oclwrs, the master FSM should gracefully go to
 *              the INIT state.
 *
 *              If the FW is in a powered down state:
 *                  *If SYSTEM_RESET is accompanied by a platform powerdown,
 *                  go directly to INIT.
 *                  *If just a SYSTEM_RESET, power up normally
 *
 *         PEX_RST is also asserted low when the platform enters D3_COLD for GOLD
 *
 *
 *  SEL:
 *      GPIO select from the designator pins
 *  INVALID:
 *      GPIO input function is not valid. Don't trust it.
 *  ILWERT:
 *      Treat signal as ilwerted (active low) assert, or active high assert
 *  STATE:
 *      Reflects the signal level after the optional ilwersion and invalid.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_PEX_RST_CFG                    0x000000d8 /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_PEX_RST_CFG_SEL                       4:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_PEX_RST_CFG_SEL_GPIO_0         0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_PEX_RST_CFG_SEL_GPIO_1         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_PEX_RST_CFG_ILWERT                    5:5 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_PEX_RST_CFG_ILWERT_NO          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_PEX_RST_CFG_ILWERT_YES         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_PEX_RST_CFG_ILWALID                   6:6 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_PEX_RST_CFG_ILWALID_NO         0x00000000 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_PEX_RST_CFG_ILWALID_YES        0x00000001 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_PEX_RST_CFG_STATE                     7:7 /* R--VF */
#define LW_EC_INFO_GPIO_INPUT_PEX_RST_CFG_STATE_DEASSERTED   0x00000000 /* R---V */
#define LW_EC_INFO_GPIO_INPUT_PEX_RST_CFG_STATE_ASSERTED     0x00000001 /* R---V */

/********************************************************************************
 *
 *  TYPE:
 *      Attach an interrupt to this input function. Interrupts are evaluated
 *      after ilwersion and invalid.
 *
 *      NONE - No interrupt
 *      LOW -  Level interrupt. Assert interrupt if signal is de-asserted
 *      HIGH - Level interrupt. Assert interrupt if signal is asserted.
 *      RISING  -  Edge interrupt. Assert interrupt if old state was DEASSERTED
 *                 and new state is ASSERTED
 *      FALLING -  Edge interrupt. Assert interrupt if old state was ASSERTED
 *                 and new state is DEASSERTED
 *      RISEFALL - Edge interrupt. Assert interrupt on any state change
 *      ANLG_GTE - Analog interrupt. Assert interrupt if voltage is greater than
 *                 or equal to the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_LT  - Analog interrupt. Assert interrupt if the voltage is less
 *                 than the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_CHANGE - Analog equivalent to RISEFALL. Assert interrupt on state
 *                    change, where a change in state is relative to a threshold.
 *  STATUS:
 *      PENDING - There is a pending interrupt. Service it.
 *      IDLE - There is no pending interrupt.
 *      CLEAR - Once the interrupt is serviced, clear it by writing 0.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_PEX_RST_INTR                    0x000000d9 /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_PEX_RST_INTR_TYPE                      3:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_PEX_RST_INTR_TYPE_NONE          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_PEX_RST_INTR_TYPE_LOW           0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_PEX_RST_INTR_TYPE_HIGH          0x00000002 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_PEX_RST_INTR_TYPE_RISING        0x00000003 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_PEX_RST_INTR_TYPE_FALLING       0x00000004 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_PEX_RST_INTR_TYPE_RISEFALL      0x00000005 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_PEX_RST_INTR_TYPE_ANLG_GTE      0x00000006 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_PEX_RST_INTR_TYPE_ANLG_LT       0x00000007 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_PEX_RST_INTR_STATUS                    7:7 /* RWEVF */
#define LW_EC_INFO_GPIO_INPUT_PEX_RST_INTR_STATUS_IDLE        0x00000000 /* RWE-V */
#define LW_EC_INFO_GPIO_INPUT_PEX_RST_INTR_STATUS_CLEAR       0x00000001 /* -W--T */
#define LW_EC_INFO_GPIO_INPUT_PEX_RST_INTR_STATUS_PENDING     0x00000001 /* R---V */

/********************************************************************************
 *
 *LW_EC_INFO_GPIO_INPUT_GPU_RST_CFG:  GPU_RST# function
 *
 * GPU_RST# is the state of reset observed by the GPU. It's an active low signal.
 *
 * GPU_RST# will be asserted when the platform is in reset, or when the GPU is
 * in D3_COLD (GOLD) and GC6. In GC6M, this signal can be used to determine when
 * GC6 has been entered by the GPU.
 *
 *  SEL:
 *      GPIO select from the designator pins
 *  INVALID:
 *      GPIO input function is not valid. Don't trust it.
 *  ILWERT:
 *      Treat signal as ilwerted (active low) assert, or active high assert
 *  STATE:
 *      Reflects the signal level after the optional ilwersion and invalid.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_GPU_RST_CFG                    0x000000da /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_CFG_SEL                       4:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_CFG_SEL_GPIO_0         0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_CFG_SEL_GPIO_1         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_CFG_ILWERT                    5:5 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_CFG_ILWERT_NO          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_CFG_ILWERT_YES         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_CFG_ILWALID                   6:6 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_CFG_ILWALID_NO         0x00000000 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_CFG_ILWALID_YES        0x00000001 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_CFG_STATE                     7:7 /* R--VF */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_CFG_STATE_DEASSERTED   0x00000000 /* R---V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_CFG_STATE_ASSERTED     0x00000001 /* R---V */

/********************************************************************************
 *
 *  TYPE:
 *      Attach an interrupt to this input function. Interrupts are evaluated
 *      after ilwersion and invalid.
 *
 *      NONE - No interrupt
 *      LOW -  Level interrupt. Assert interrupt if signal is de-asserted
 *      HIGH - Level interrupt. Assert interrupt if signal is asserted.
 *      RISING  -  Edge interrupt. Assert interrupt if old state was DEASSERTED
 *                 and new state is ASSERTED
 *      FALLING -  Edge interrupt. Assert interrupt if old state was ASSERTED
 *                 and new state is DEASSERTED
 *      RISEFALL - Edge interrupt. Assert interrupt on any state change
 *      ANLG_GTE - Analog interrupt. Assert interrupt if voltage is greater than
 *                 or equal to the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_LT  - Analog interrupt. Assert interrupt if the voltage is less
 *                 than the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_CHANGE - Analog equivalent to RISEFALL. Assert interrupt on state
 *                    change, where a change in state is relative to a threshold.
 *  STATUS:
 *      PENDING - There is a pending interrupt. Service it.
 *      IDLE - There is no pending interrupt.
 *      CLEAR - Once the interrupt is serviced, clear it by writing 0.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_GPU_RST_INTR                    0x000000db /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_INTR_TYPE                      3:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_INTR_TYPE_NONE          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_INTR_TYPE_LOW           0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_INTR_TYPE_HIGH          0x00000002 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_INTR_TYPE_RISING        0x00000003 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_INTR_TYPE_FALLING       0x00000004 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_INTR_TYPE_RISEFALL      0x00000005 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_INTR_TYPE_ANLG_GTE      0x00000006 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_INTR_TYPE_ANLG_LT       0x00000007 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_INTR_TYPE_ANLG_CHANGE   0x00000008 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_INTR_STATUS                    7:7 /* RWEVF */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_INTR_STATUS_IDLE        0x00000000 /* RWE-V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_INTR_STATUS_CLEAR       0x00000001 /* -W--T */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_INTR_STATUS_PENDING     0x00000001 /* R---V */

/********************************************************************************
 *
 *LW_EC_INFO_GPIO_INPUT_GPU_RST_HOLD_CFG:  GPU_RST_HOLD# function
 *
 * GPU_RST_HOLD# is an output signal from the GPU, specific to GC6M.
 * It's an active low signal. When GPU_RST_HOLD# is asserted, this indicates that
 * the GPU has put itself into GC6. The Master FSM should move to GC6M_PWRDN_RST
 * if other preconditions are met.
 *
 * If GPU_RST_HOLD# is asserted and GC6M mode is not selected, this is an error.
 *
 *
 *  SEL:
 *      GPIO select from the designator pins
 *  INVALID:
 *      GPIO input function is not valid. Don't trust it.
 *  ILWERT:
 *      Treat signal as ilwerted (active low) assert, or active high assert
 *  STATE:
 *      Reflects the signal level after the optional ilwersion and invalid.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_GPU_RST_HOLD_CFG                    0x000000dc /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_HOLD_CFG_SEL                       4:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_HOLD_CFG_SEL_GPIO_0         0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_HOLD_CFG_SEL_GPIO_1         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_HOLD_CFG_ILWERT                    5:5 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_HOLD_CFG_ILWERT_NO          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_HOLD_CFG_ILWERT_YES         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_HOLD_CFG_ILWALID                   6:6 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_HOLD_CFG_ILWALID_NO         0x00000000 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_HOLD_CFG_ILWALID_YES        0x00000001 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_HOLD_CFG_STATE                     7:7 /* R--VF */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_HOLD_CFG_STATE_DEASSERTED   0x00000000 /* R---V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_HOLD_CFG_STATE_ASSERTED     0x00000001 /* R---V */

/********************************************************************************
 *
 *  TYPE:
 *      Attach an interrupt to this input function. Interrupts are evaluated
 *      after ilwersion and invalid.
 *
 *      NONE - No interrupt
 *      LOW -  Level interrupt. Assert interrupt if signal is de-asserted
 *      HIGH - Level interrupt. Assert interrupt if signal is asserted.
 *      RISING  -  Edge interrupt. Assert interrupt if old state was DEASSERTED
 *                 and new state is ASSERTED
 *      FALLING -  Edge interrupt. Assert interrupt if old state was ASSERTED
 *                 and new state is DEASSERTED
 *      RISEFALL - Edge interrupt. Assert interrupt on any state change
 *      ANLG_GTE - Analog interrupt. Assert interrupt if voltage is greater than
 *                 or equal to the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_LT  - Analog interrupt. Assert interrupt if the voltage is less
 *                 than the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_CHANGE - Analog equivalent to RISEFALL. Assert interrupt on state
 *                    change, where a change in state is relative to a threshold.
 *  STATUS:
 *      PENDING - There is a pending interrupt. Service it.
 *      IDLE - There is no pending interrupt.
 *      CLEAR - Once the interrupt is serviced, clear it by writing 0.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_GPU_RST_HOLD_INTR                    0x000000dd /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_HOLD_INTR_TYPE                      3:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_HOLD_INTR_TYPE_NONE          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_HOLD_INTR_TYPE_LOW           0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_HOLD_INTR_TYPE_HIGH          0x00000002 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_HOLD_INTR_TYPE_RISING        0x00000003 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_HOLD_INTR_TYPE_FALLING       0x00000004 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_HOLD_INTR_TYPE_RISEFALL      0x00000005 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_HOLD_INTR_TYPE_ANLG_GTE      0x00000006 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_HOLD_INTR_TYPE_ANLG_LT       0x00000007 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_HOLD_INTR_TYPE_ANLG_CHANGE   0x00000008 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_HOLD_INTR_STATUS                    7:7 /* RWEVF */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_HOLD_INTR_STATUS_IDLE        0x00000000 /* RWE-V */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_HOLD_INTR_STATUS_CLEAR       0x00000001 /* -W--T */
#define LW_EC_INFO_GPIO_INPUT_GPU_RST_HOLD_INTR_STATUS_PENDING     0x00000001 /* R---V */

/********************************************************************************
 *
 *LW_EC_INFO_GPIO_INPUT_GC5_MODE_CFG:  GPU_RST_HOLD# function
 *
 * GC5_MODE is an output signal from the GPU, specific to GC6M. When asserted,
 * the gpu is in GC5. To the master FSM, there is no special behavior needed
 * at this time. It may be interesting to de-assert PGOOD on this event, but
 * that is not planned for now. It may also be interesting to log GC5 for MODS
 * Need to clear that up with the MODS team first.
 *
 *
 *  SEL:
 *      GPIO select from the designator pins
 *  INVALID:
 *      GPIO input function is not valid. Don't trust it.
 *  ILWERT:
 *      Treat signal as ilwerted (active low) assert, or active high assert
 *  STATE:
 *      Reflects the signal level after the optional ilwersion and invalid.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_GC5_MODE_CFG                    0x000000de /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_GC5_MODE_CFG_SEL                       4:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_GC5_MODE_CFG_SEL_GPIO_0         0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_GC5_MODE_CFG_SEL_GPIO_1         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GC5_MODE_CFG_ILWERT                    5:5 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_GC5_MODE_CFG_ILWERT_NO          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_GC5_MODE_CFG_ILWERT_YES         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GC5_MODE_CFG_ILWALID                   6:6 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_GC5_MODE_CFG_ILWALID_NO         0x00000000 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GC5_MODE_CFG_ILWALID_YES        0x00000001 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_GC5_MODE_CFG_STATE                     7:7 /* R--VF */
#define LW_EC_INFO_GPIO_INPUT_GC5_MODE_CFG_STATE_DEASSERTED   0x00000000 /* R---V */
#define LW_EC_INFO_GPIO_INPUT_GC5_MODE_CFG_STATE_ASSERTED     0x00000001 /* R---V */

/********************************************************************************
 *
 *  TYPE:
 *      Attach an interrupt to this input function. Interrupts are evaluated
 *      after ilwersion and invalid.
 *
 *      NONE - No interrupt
 *      LOW -  Level interrupt. Assert interrupt if signal is de-asserted
 *      HIGH - Level interrupt. Assert interrupt if signal is asserted.
 *      RISING  -  Edge interrupt. Assert interrupt if old state was DEASSERTED
 *                 and new state is ASSERTED
 *      FALLING -  Edge interrupt. Assert interrupt if old state was ASSERTED
 *                 and new state is DEASSERTED
 *      RISEFALL - Edge interrupt. Assert interrupt on any state change
 *      ANLG_GTE - Analog interrupt. Assert interrupt if voltage is greater than
 *                 or equal to the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_LT  - Analog interrupt. Assert interrupt if the voltage is less
 *                 than the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_CHANGE - Analog equivalent to RISEFALL. Assert interrupt on state
 *                    change, where a change in state is relative to a threshold.
 *  STATUS:
 *      PENDING - There is a pending interrupt. Service it.
 *      IDLE - There is no pending interrupt.
 *      CLEAR - Once the interrupt is serviced, clear it by writing 0.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_GC5_MODE_INTR                    0x000000df /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_GC5_MODE_INTR_TYPE                      3:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_GC5_MODE_INTR_TYPE_NONE          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_GC5_MODE_INTR_TYPE_LOW           0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GC5_MODE_INTR_TYPE_HIGH          0x00000002 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GC5_MODE_INTR_TYPE_RISING        0x00000003 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GC5_MODE_INTR_TYPE_FALLING       0x00000004 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GC5_MODE_INTR_TYPE_RISEFALL      0x00000005 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GC5_MODE_INTR_TYPE_ANLG_GTE      0x00000006 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GC5_MODE_INTR_TYPE_ANLG_LT       0x00000007 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GC5_MODE_INTR_TYPE_ANLG_CHANGE   0x00000008 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GC5_MODE_INTR_STATUS                    7:7 /* RWEVF */
#define LW_EC_INFO_GPIO_INPUT_GC5_MODE_INTR_STATUS_IDLE        0x00000000 /* RWE-V */
#define LW_EC_INFO_GPIO_INPUT_GC5_MODE_INTR_STATUS_CLEAR       0x00000001 /* -W--T */
#define LW_EC_INFO_GPIO_INPUT_GC5_MODE_INTR_STATUS_PENDING     0x00000001 /* R---V */

/********************************************************************************
 *
 *LW_EC_INFO_GPIO_INPUT_HPD_CFG:  Hotplug Detect Function
 *
 * HPD should reflect the state of attached panels to the e-board.
 *
 * HPD is a source of wake-interrupts when the GPU is in GC6K or GC6M. Once
 * HPD has changed state (plug to unplug or vice versa), the master FSM should
 * exit out to the GC6_PWRUP_REQ state, GC6M_PWRUP_REQ, or D3_PWRUP_RST states.
 *
 * On GC6M it's expected that the GPU will service the wake-interrupt for HPD
 * itself.
 *
 * Note that HPD is a little bit special because it may be desired to delay the
 * servicing of the interrupt. For that, please refer to GPIO_HPD_WAKE_FILTER,
 * which can delay servicing of HPD so that the GPU can service the HPD instead,
 * and issue a GPU_EVENT# interrupt back to the EC.
 *
 *  SEL:
 *      GPIO select from the designator pins
 *  INVALID:
 *      GPIO input function is not valid. Don't trust it.
 *  ILWERT:
 *      Treat signal as ilwerted (active low) assert, or active high assert
 *  STATE:
 *      Reflects the signal level after the optional ilwersion and invalid.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_HPD_CFG(i)       (0x000000e0 + (i)*2) /* RW-1A */
#define LW_EC_INFO_GPIO_INPUT_HPD_CFG__SIZE_1                     6 /*       */
#define LW_EC_INFO_GPIO_INPUT_HPD_CFG_SEL                       4:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_HPD_CFG_SEL_GPIO_0         0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_HPD_CFG_SEL_GPIO_1         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_HPD_CFG_ILWERT                    5:5 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_HPD_CFG_ILWERT_NO          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_HPD_CFG_ILWERT_YES         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_HPD_CFG_ILWALID                   6:6 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_HPD_CFG_ILWALID_NO         0x00000000 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_HPD_CFG_ILWALID_YES        0x00000001 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_HPD_CFG_STATE                     7:7 /* R--VF */
#define LW_EC_INFO_GPIO_INPUT_HPD_CFG_STATE_DEASSERTED   0x00000000 /* R---V */
#define LW_EC_INFO_GPIO_INPUT_HPD_CFG_STATE_ASSERTED     0x00000001 /* R---V */

/********************************************************************************
 *
 *  TYPE:
 *      Attach an interrupt to this input function. Interrupts are evaluated
 *      after ilwersion and invalid.
 *
 *      NONE - No interrupt
 *      LOW -  Level interrupt. Assert interrupt if signal is de-asserted
 *      HIGH - Level interrupt. Assert interrupt if signal is asserted.
 *      RISING  -  Edge interrupt. Assert interrupt if old state was DEASSERTED
 *                 and new state is ASSERTED
 *      FALLING -  Edge interrupt. Assert interrupt if old state was ASSERTED
 *                 and new state is DEASSERTED
 *      RISEFALL - Edge interrupt. Assert interrupt on any state change
 *      ANLG_GTE - Analog interrupt. Assert interrupt if voltage is greater than
 *                 or equal to the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_LT  - Analog interrupt. Assert interrupt if the voltage is less
 *                 than the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_CHANGE - Analog equivalent to RISEFALL. Assert interrupt on state
 *                    change, where a change in state is relative to a threshold.
 *  STATUS:
 *      PENDING - There is a pending interrupt. Service it.
 *      IDLE - There is no pending interrupt.
 *      CLEAR - Once the interrupt is serviced, clear it by writing 0.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_HPD_INTR(i)       (0x000000e1 + (i)*2) /* RW-1A */
#define LW_EC_INFO_GPIO_INPUT_HPD_INTR__SIZE_1                     6 /*       */
#define LW_EC_INFO_GPIO_INPUT_HPD_INTR_TYPE                      3:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_HPD_INTR_TYPE_NONE          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_HPD_INTR_TYPE_LOW           0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_HPD_INTR_TYPE_HIGH          0x00000002 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_HPD_INTR_TYPE_RISING        0x00000003 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_HPD_INTR_TYPE_FALLING       0x00000004 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_HPD_INTR_TYPE_RISEFALL      0x00000005 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_HPD_INTR_TYPE_ANLG_GTE      0x00000006 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_HPD_INTR_TYPE_ANLG_LT       0x00000007 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_HPD_INTR_TYPE_ANLG_CHANGE   0x00000008 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_HPD_INTR_STATUS                    7:7 /* RWEVF */
#define LW_EC_INFO_GPIO_INPUT_HPD_INTR_STATUS_IDLE        0x00000000 /* RWE-V */
#define LW_EC_INFO_GPIO_INPUT_HPD_INTR_STATUS_CLEAR       0x00000001 /* -W--T */
#define LW_EC_INFO_GPIO_INPUT_HPD_INTR_STATUS_PENDING     0x00000001 /* R---V */

/********************************************************************************
 *
 *LW_EC_INFO_GPIO_INPUT_GC6_FB_EN_CFG:  GC6_FB_EN configuration
 *
 * GC6_FB_EN is an output signal from the GPU. It is mapped to the same
 * physical pin as FB_CLAMP. When asserted, the gpu is in GC6. When it
 * de-asserts, the GPU has exited GC6M. It can be used for wake-events
 * similarly to GPU_EVENT. Customer platforms that cannot support
 * bi-directional GPU_EVENT can use this signal to support GPU-generated wake
 * events.
 *
 *  SEL:
 *      GPIO select from the designator pins
 *  INVALID:
 *      GPIO input function is not valid. Don't trust it.
 *  ILWERT:
 *      Treat signal as ilwerted (active low) assert, or active high assert
 *  STATE:
 *      Reflects the signal level after the optional ilwersion and invalid.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_GC6_FB_EN_CFG                    0x000000f0 /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_GC6_FB_EN_CFG_SEL                       4:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_GC6_FB_EN_CFG_SEL_GPIO_0         0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_GC6_FB_EN_CFG_SEL_GPIO_1         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GC6_FB_EN_CFG_ILWERT                    5:5 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_GC6_FB_EN_CFG_ILWERT_NO          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_GC6_FB_EN_CFG_ILWERT_YES         0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GC6_FB_EN_CFG_ILWALID                   6:6 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_GC6_FB_EN_CFG_ILWALID_NO         0x00000000 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GC6_FB_EN_CFG_ILWALID_YES        0x00000001 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_GC6_FB_EN_CFG_STATE                     7:7 /* R--VF */
#define LW_EC_INFO_GPIO_INPUT_GC6_FB_EN_CFG_STATE_DEASSERTED   0x00000000 /* R---V */
#define LW_EC_INFO_GPIO_INPUT_GC6_FB_EN_CFG_STATE_ASSERTED     0x00000001 /* R---V */

/********************************************************************************
 *
 *  TYPE:
 *      Attach an interrupt to this input function. Interrupts are evaluated
 *      after ilwersion and invalid.
 *
 *      NONE - No interrupt
 *      LOW -  Level interrupt. Assert interrupt if signal is de-asserted
 *      HIGH - Level interrupt. Assert interrupt if signal is asserted.
 *      RISING  -  Edge interrupt. Assert interrupt if old state was DEASSERTED
 *                 and new state is ASSERTED
 *      FALLING -  Edge interrupt. Assert interrupt if old state was ASSERTED
 *                 and new state is DEASSERTED
 *      RISEFALL - Edge interrupt. Assert interrupt on any state change
 *      ANLG_GTE - Analog interrupt. Assert interrupt if voltage is greater than
 *                 or equal to the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_LT  - Analog interrupt. Assert interrupt if the voltage is less
 *                 than the threshold (In ANLG_0 and ANLG_1).
 *      ANLG_CHANGE - Analog equivalent to RISEFALL. Assert interrupt on state
 *                    change, where a change in state is relative to a threshold.
 *  STATUS:
 *      PENDING - There is a pending interrupt. Service it.
 *      IDLE - There is no pending interrupt.
 *      CLEAR - Once the interrupt is serviced, clear it by writing 0.
 *
 * *****************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_GC6_FB_EN_INTR                    0x000000f1 /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_GC6_FB_EN_INTR_TYPE                      3:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_GC6_FB_EN_INTR_TYPE_NONE          0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_GC6_FB_EN_INTR_TYPE_LOW           0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GC6_FB_EN_INTR_TYPE_HIGH          0x00000002 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GC6_FB_EN_INTR_TYPE_RISING        0x00000003 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GC6_FB_EN_INTR_TYPE_FALLING       0x00000004 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GC6_FB_EN_INTR_TYPE_RISEFALL      0x00000005 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GC6_FB_EN_INTR_TYPE_ANLG_GTE      0x00000006 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GC6_FB_EN_INTR_TYPE_ANLG_LT       0x00000007 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GC6_FB_EN_INTR_TYPE_ANLG_CHANGE   0x00000008 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GC6_FB_EN_INTR_STATUS                    7:7 /* RWEVF */
#define LW_EC_INFO_GPIO_INPUT_GC6_FB_EN_INTR_STATUS_IDLE        0x00000000 /* RWE-V */
#define LW_EC_INFO_GPIO_INPUT_GC6_FB_EN_INTR_STATUS_CLEAR       0x00000001 /* -W--T */
#define LW_EC_INFO_GPIO_INPUT_GC6_FB_EN_INTR_STATUS_PENDING     0x00000001 /* R---V */

/******************************************************************************
* GPIO_INPUT_GPU_VIS_WAKE_DELAY
*
* Once the interrupt for an GPU-visible interrupt (HPD, PWR_ALERT) asserts,
* apply a filter at the end to delay the Master FSM from trying to exit right
* away. This will ensure good coverage on GC6M while unifying the interface for
* D3_COLD/GOLD and GC6.
*
* total delay = SCALE*NUM_STEPS.
*
* SCALE:
*    10US -  Each step is 10uS. Range of 0uS to 630uS
*    100US - Each step is 100uS. Range of 0uS to 6300uS
*    1MS   - Each step is 1mS. Range of 0mS to 63mS
*    10MS -  Each step is 10mS. Range of 0mS to 630mS
*
* NUM_STEPS:
*    Number of steps to delay
******************************************************************************/

#define LW_EC_INFO_GPIO_INPUT_GPU_VIS_WAKE_DELAY                      0x000000f4 /* RW-1R */
#define LW_EC_INFO_GPIO_INPUT_GPU_VIS_WAKE_DELAY_SCALE                       1:0 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_GPU_VIS_WAKE_DELAY_SCALE_10US           0x00000000 /* RWI-V */
#define LW_EC_INFO_GPIO_INPUT_GPU_VIS_WAKE_DELAY_SCALE_100US          0x00000001 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_VIS_WAKE_DELAY_SCALE_1MS            0x00000002 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_VIS_WAKE_DELAY_SCALE_10MS           0x00000003 /* RW--V */
#define LW_EC_INFO_GPIO_INPUT_GPU_VIS_WAKE_DELAY_NUM_STEPS                   7:2 /* RWIVF */
#define LW_EC_INFO_GPIO_INPUT_GPU_VIS_WAKE_DELAY_NUM_STEPS_0          0x00000000 /* RWI-V */

/******************************************************************************
The power sequencer outputs are double-buffered. The ARMED state for each
power sequencer signal represents its "pending" value. These outputs are not
directly observed at the GPIO crossbar. The ACTIVE state represents the output
being driven to the GPIO crossbar and can be observed on the EC IO pins.

When an update is triggered, the ARMED state is copied to the ACTIVE state.
Triggers can be initiated by the power sequence itself, or by writing to
register __ __.

Signals:
    GPU_EVENT
    VR_EN_3V3
    VR_EN_LWVDD
    VR_EN_VRAM_TOTAL
    VR_EN_GPU_FBIO
    GPU_RST
    PGOOD
    PWR_ALERT
    MISC0
    MISC1
******************************************************************************/

/******************************************************************************
* ARMED State. Pending output from the power sequencer
******************************************************************************/

#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_0                           0x000000f6 /* RW-1R */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_0_GPU_EVENT                        0:0 /* RWIVF */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_0_GPU_EVENT_DEASSERT        0x00000000 /* RWI-V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_0_GPU_EVENT_ASSERT          0x00000001 /* RW--V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_0_VR_EN_3V3                        1:1 /* RWIVF */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_0_VR_EN_3V3_DEASSERT        0x00000000 /* RWI-V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_0_VR_EN_3V3_ASSERT          0x00000001 /* RW--V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_0_VR_EN_LWVDD                      2:2 /* RWIVF */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_0_VR_EN_LWVDD_DEASSERT      0x00000000 /* RWI-V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_0_VR_EN_LWVDD_ASSERT        0x00000001 /* RW--V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_0_VR_EN_VRAM_TOTAL                 3:3 /* RWIVF */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_0_VR_EN_VRAM_TOTAL_DEASSERT 0x00000000 /* RWI-V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_0_VR_EN_VRAM_TOTAL_ASSERT   0x00000001 /* RW--V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_0_VR_EN_GPU_FBIO                   4:4 /* RWIVF */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_0_VR_EN_GPU_FBIO_DEASSERT   0x00000000 /* RWI-V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_0_VR_EN_GPU_FBIO_ASSERT     0x00000001 /* RW--V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_0_VR_EN_1V                         5:5 /* RWIVF */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_0_VR_EN_1V_DEASSERT         0x00000000 /* RWI-V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_0_VR_EN_1V_ASSERT           0x00000001 /* RW--V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_0_EC_GPU_RST                       6:6 /* RWIVF */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_0_EC_GPU_RST_DEASSERT       0x00000000 /* RWI-V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_0_EC_GPU_RST_ASSERT         0x00000001 /* RW--V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_0_PGOOD                            7:7 /* RWIVF */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_0_PGOOD_DEASSERT            0x00000000 /* RWI-V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_0_PGOOD_ASSERT              0x00000001 /* RW--V */

#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_1                           0x000000f7 /* RW-1R */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_1_PWR_ALERT                        0:0 /* RWIVF */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_1_PWR_ALERT_DEASSERT        0x00000000 /* RWI-V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_1_PWR_ALERT_ASSERT          0x00000001 /* RW--V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_1_MISC0                            1:1 /* RWIVF */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_1_MISC0_DEASSERT            0x00000000 /* RWI-V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_1_MISC0_ASSERT              0x00000001 /* RW--V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_1_MISC1                            2:2 /* RWIVF */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_1_MISC1_DEASSERT            0x00000000 /* RWI-V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ARMED_1_MISC1_ASSERT              0x00000001 /* RW--V */

/******************************************************************************
* ACTIVE State. Current output from the power sequencer
******************************************************************************/

#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_0                           0x000000f9 /* RW-1R */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_0_GPU_EVENT                        0:0 /* RWIVF */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_0_GPU_EVENT_DEASSERT        0x00000000 /* RWI-V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_0_GPU_EVENT_ASSERT          0x00000001 /* RW--V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_0_VR_EN_3V3                        1:1 /* RWIVF */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_0_VR_EN_3V3_DEASSERT        0x00000000 /* RWI-V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_0_VR_EN_3V3_ASSERT          0x00000001 /* RW--V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_0_VR_EN_LWVDD                      2:2 /* RWIVF */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_0_VR_EN_LWVDD_DEASSERT      0x00000000 /* RWI-V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_0_VR_EN_LWVDD_ASSERT        0x00000001 /* RW--V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_0_VR_EN_VRAM_TOTAL                 3:3 /* RWIVF */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_0_VR_EN_VRAM_TOTAL_DEASSERT 0x00000000 /* RWI-V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_0_VR_EN_VRAM_TOTAL_ASSERT   0x00000001 /* RW--V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_0_VR_EN_GPU_FBIO                   4:4 /* RWIVF */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_0_VR_EN_GPU_FBIO_DEASSERT   0x00000000 /* RWI-V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_0_VR_EN_GPU_FBIO_ASSERT     0x00000001 /* RW--V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_0_VR_EN_1V                         5:5 /* RWIVF */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_0_VR_EN_1V_DEASSERT         0x00000000 /* RWI-V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_0_VR_EN_1V_ASSERT           0x00000001 /* RW--V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_0_EC_GPU_RST                       6:6 /* RWIVF */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_0_EC_GPU_RST_DEASSERT       0x00000000 /* RWI-V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_0_EC_GPU_RST_ASSERT         0x00000001 /* RW--V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_0_PGOOD                            7:7 /* RWIVF */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_0_PGOOD_DEASSERT            0x00000000 /* RWI-V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_0_PGOOD_ASSERT              0x00000001 /* RW--V */

#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_1                           0x000000fa /* RW-1R */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_1_PWR_ALERT                        0:0 /* RWIVF */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_1_PWR_ALERT_DEASSERT        0x00000000 /* RWI-V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_1_PWR_ALERT_ASSERT          0x00000001 /* RW--V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_1_MISC0                            1:1 /* RWIVF */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_1_MISC0_DEASSERT            0x00000000 /* RWI-V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_1_MISC0_ASSERT              0x00000001 /* RW--V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_1_MISC1                            2:2 /* RWIVF */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_1_MISC1_DEASSERT            0x00000000 /* RWI-V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_ACTIVE_1_MISC1_ASSERT              0x00000001 /* RW--V */

/******************************************************************************
* PWR_SEQ_OUTPUT - Copy value in PWR_SEQ_OUTPUT_ARMED to PWR_SEQ_OUTPUT_ACTIIVE
******************************************************************************/

#define LW_EC_INFO_PWR_SEQ_OUTPUT                        0x000000ff /* RW-1R */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_UPDATE                        0:0 /* RWEVF */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_UPDATE_DONE            0x00000000 /* RWE-V */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_UPDATE_TRIGGER         0x00000001 /* -W--T */
#define LW_EC_INFO_PWR_SEQ_OUTPUT_UPDATE_PENDING         0x00000001 /* R---V */

/********************************************************************************
* MASTER_FSM_TIMEOUT_MS: Maximum time that can be spent in a given state.
*
*   The purpose of this register is to help catch errors without hanging the
*   Master FSM (this could corrupt SW). Terminal states (like FULL_PWR, D3_COLD,
*   GC6, and GC6M should have a value of 0 to disable the timeout. Transitional
*   states should have a timeout. 10MS should be a decent starting point for
*   most states.
*
* TIMEOUT: The timeout value, in miliseconds for the master FSM state.
*   - TIMEOUT_DISABLED: The FW should not configure a counter with a timeout.
********************************************************************************/

#define LW_EC_INFO_MASTER_FSM_TIMEOUT_MS(i)           (0x00000100 + i) /* RW-1A */
#define LW_EC_INFO_MASTER_FSM_TIMEOUT_MS__SIZE_1                    32 /*       */
#define LW_EC_INFO_MASTER_FSM_TIMEOUT_MS_TIMEOUT                   7:0 /* RWIVF */
#define LW_EC_INFO_MASTER_FSM_TIMEOUT_MS_TIMEOUT_DISABLED   0x00000000 /* RWI-V */
#define LW_EC_INFO_MASTER_FSM_TIMEOUT_MS_TIMEOUT_10MS       0x0000000a /* RW--V */

//aliases
#define LW_EC_INFO_MASTER_FSM_TIMEOUT_MS_RESET                            LW_EC_INFO_MASTER_FSM_TIMEOUT_MS(LW_EC_INFO_DEBUG_0_FSM_STATE_RESET) /* RW-1R */
#define LW_EC_INFO_MASTER_FSM_TIMEOUT_MS_RESET_NO_PWR              LW_EC_INFO_MASTER_FSM_TIMEOUT_MS(LW_EC_INFO_DEBUG_0_FSM_STATE_RESET_NO_PWR) /* RW-1R */
#define LW_EC_INFO_MASTER_FSM_TIMEOUT_MS_FULL_PWR                      LW_EC_INFO_MASTER_FSM_TIMEOUT_MS(LW_EC_INFO_DEBUG_0_FSM_STATE_FULL_PWR) /* RW-1R */
#define LW_EC_INFO_MASTER_FSM_TIMEOUT_MS_D3_PWRDN_RST              LW_EC_INFO_MASTER_FSM_TIMEOUT_MS(LW_EC_INFO_DEBUG_0_FSM_STATE_D3_PWRDN_RST) /* RW-1R */
#define LW_EC_INFO_MASTER_FSM_TIMEOUT_MS_D3_COLD                        LW_EC_INFO_MASTER_FSM_TIMEOUT_MS(LW_EC_INFO_DEBUG_0_FSM_STATE_D3_COLD) /* RW-1R */
#define LW_EC_INFO_MASTER_FSM_TIMEOUT_MS_D3_PWRUP_RST              LW_EC_INFO_MASTER_FSM_TIMEOUT_MS(LW_EC_INFO_DEBUG_0_FSM_STATE_D3_PWRUP_RST) /* RW-1R */
#define LW_EC_INFO_MASTER_FSM_TIMEOUT_MS_GC6_PWRDN_RST            LW_EC_INFO_MASTER_FSM_TIMEOUT_MS(LW_EC_INFO_DEBUG_0_FSM_STATE_GC6_PWRDN_RST) /* RW-1R */
#define LW_EC_INFO_MASTER_FSM_TIMEOUT_MS_GC6                                LW_EC_INFO_MASTER_FSM_TIMEOUT_MS(LW_EC_INFO_DEBUG_0_FSM_STATE_GC6) /* RW-1R */
#define LW_EC_INFO_MASTER_FSM_TIMEOUT_MS_GC6_PWRUP_REQ            LW_EC_INFO_MASTER_FSM_TIMEOUT_MS(LW_EC_INFO_DEBUG_0_FSM_STATE_GC6_PWRUP_REQ) /* RW-1R */
#define LW_EC_INFO_MASTER_FSM_TIMEOUT_MS_GC6M_PWRDN_RST          LW_EC_INFO_MASTER_FSM_TIMEOUT_MS(LW_EC_INFO_DEBUG_0_FSM_STATE_GC6M_PWRDN_RST) /* RW-1R */
#define LW_EC_INFO_MASTER_FSM_TIMEOUT_MS_GC6M                              LW_EC_INFO_MASTER_FSM_TIMEOUT_MS(LW_EC_INFO_DEBUG_0_FSM_STATE_GC6M) /* RW-1R */
#define LW_EC_INFO_MASTER_FSM_TIMEOUT_MS_GC6M_PWRUP_REQ          LW_EC_INFO_MASTER_FSM_TIMEOUT_MS(LW_EC_INFO_DEBUG_0_FSM_STATE_GC6M_PWRUP_REQ) /* RW-1R */
#define LW_EC_INFO_MASTER_FSM_TIMEOUT_MS_GC6M_PWRUP_SR_UPD    LW_EC_INFO_MASTER_FSM_TIMEOUT_MS(LW_EC_INFO_DEBUG_0_FSM_STATE_GC6M_PWRUP_SR_UPD) /* RW-1R */
#define LW_EC_INFO_MASTER_FSM_TIMEOUT_MS_GC6M_PWRUP_T1            LW_EC_INFO_MASTER_FSM_TIMEOUT_MS(LW_EC_INFO_DEBUG_0_FSM_STATE_GC6M_PWRUP_T1) /* RW-1R */
#define LW_EC_INFO_MASTER_FSM_TIMEOUT_MS_GC6M_PWRUP_T2            LW_EC_INFO_MASTER_FSM_TIMEOUT_MS(LW_EC_INFO_DEBUG_0_FSM_STATE_GC6M_PWRUP_T2) /* RW-1R */

/******************************************************************************
PWR_SEQ_ENTRY_VECTOR

PWR_SEQ_ENTRY_VECTOR_<Master FSM State> - Each Master FSM state has a pointer
        into the power sequencer instruction table. When the state is entered,
        the power sequencer is triggered to run at the program counter at value
        PC.

PC: Program counter pointer for the given entry vector.
******************************************************************************/
#define LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR(i)                 (0x000000120 + (i)) /* RW-1A */
#define LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR_PC__SIZE_1                          32 /*       */
#define LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR_PC                                 7:0 /* RWIVF */
#define LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR_PC_INIT                     0x00000000 /* RWI-V */

//aliases
#define LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR_RESET                            LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR(LW_EC_INFO_DEBUG_0_FSM_STATE_RESET) /* RW-1R */
#define LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR_RESET_NO_PWR              LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR(LW_EC_INFO_DEBUG_0_FSM_STATE_RESET_NO_PWR) /* RW-1R */
#define LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR_FULL_PWR                      LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR(LW_EC_INFO_DEBUG_0_FSM_STATE_FULL_PWR) /* RW-1R */
#define LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR_D3_PWRDN_RST              LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR(LW_EC_INFO_DEBUG_0_FSM_STATE_D3_PWRDN_RST) /* RW-1R */
#define LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR_D3_COLD                        LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR(LW_EC_INFO_DEBUG_0_FSM_STATE_D3_COLD) /* RW-1R */
#define LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR_D3_PWRUP_RST              LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR(LW_EC_INFO_DEBUG_0_FSM_STATE_D3_PWRUP_RST) /* RW-1R */
#define LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR_GC6_PWRDN_RST            LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR(LW_EC_INFO_DEBUG_0_FSM_STATE_GC6_PWRDN_RST) /* RW-1R */
#define LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR_GC6                                LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR(LW_EC_INFO_DEBUG_0_FSM_STATE_GC6) /* RW-1R */
#define LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR_GC6_PWRUP_REQ            LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR(LW_EC_INFO_DEBUG_0_FSM_STATE_GC6_PWRUP_REQ) /* RW-1R */
#define LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR_GC6M_PWRDN_RST          LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR(LW_EC_INFO_DEBUG_0_FSM_STATE_GC6M_PWRDN_RST) /* RW-1R */
#define LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR_GC6M                              LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR(LW_EC_INFO_DEBUG_0_FSM_STATE_GC6M) /* RW-1R */
#define LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR_GC6M_PWRUP_REQ          LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR(LW_EC_INFO_DEBUG_0_FSM_STATE_GC6M_PWRUP_REQ) /* RW-1R */
#define LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR_GC6M_PWRUP_SR_UPD    LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR(LW_EC_INFO_DEBUG_0_FSM_STATE_GC6M_PWRUP_SR_UPD) /* RW-1R */
#define LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR_GC6M_PWRUP_T1            LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR(LW_EC_INFO_DEBUG_0_FSM_STATE_GC6M_PWRUP_T1) /* RW-1R */
#define LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR_GC6M_PWRUP_T2            LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR(LW_EC_INFO_DEBUG_0_FSM_STATE_GC6M_PWRUP_T2) /* RW-1R */

/******************************************************************************
PWR_SEQ_ENTRY_VECTOR_SW

The SW Vector is special because it can be adjusted and triggered outside
of any particular Master FSM state. It cannot be triggered while another power
sequence is running. Trigger using LW_EC_INFO_PWR_SEQ_SW

PC: Program counter pointer for the given entry vector.
******************************************************************************/
#define LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR_SW                  LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR(31) /* RW-1R */
#define LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR_SW_PC                                               7:0 /* RWIVF */
#define LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR_SW_PC_INIT                                   0x00000000 /* RWI-V */

/******************************************************************************
PWR_SEQ_SW

Use this register to run the power seqeuncer from the PC designated in
LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR_SW_PC. After triggering, the field will be
kept as PENDING until the sequener

PC: Program counter pointer for the given entry vector.
******************************************************************************/

#define LW_EC_INFO_PWR_SEQ_SW                               LW_EC_INFO_PWR_SEQ_ENTRY_VECTOR(32) /* RW-1R */
#define LW_EC_INFO_PWR_SEQ_SW_SEQ                                                           0:0 /* RWEVF */
#define LW_EC_INFO_PWR_SEQ_SW_SEQ_READY                                              0x00000000 /* RWE-V */
#define LW_EC_INFO_PWR_SEQ_SW_SEQ_TRIGGER                                            0x00000001 /* -W--T */
#define LW_EC_INFO_PWR_SEQ_SW_SEQ_PENDING                                            0x00000001 /* R---V */

/******************************************************************************
DELAY_TABLE:

Each entry in the delay table contains a time delay

total delay = SCALE*NUM_STEPS.

SCALE:
    10US -  Each step is 10uS. Range of 0uS to 630uS
    100US - Each step is 100uS. Range of 0uS to 6300uS
    1MS   - Each step is 1mS. Range of 0mS to 63mS
    10MS -  Each step is 10mS. Range of 0mS to 630mS

NUM_STEPS:
    Number of steps to delay

NOTE - This was designed with a 50MHz clock in mind, and a 500 clock cycle
completion time for the timer interrupt, which should be very small. If the
clock cycle is longer than 50MHz,the real minimum delay might be longer.
*******************************************************************************/

#define LW_EC_INFO_PWR_SEQ_DELAY_TABLE(i)           (0x00000144 + (i)) /* RW-1A */
#define LW_EC_INFO_PWR_SEQ_DELAY_TABLE__SIZE_1                      16 /*       */
#define LW_EC_INFO_PWR_SEQ_DELAY_TABLE_SCALE                       1:0 /* RWIVF */
#define LW_EC_INFO_PWR_SEQ_DELAY_TABLE_SCALE_10US           0x00000000 /* RWI-V */
#define LW_EC_INFO_PWR_SEQ_DELAY_TABLE_SCALE_100US          0x00000001 /* RW--V */
#define LW_EC_INFO_PWR_SEQ_DELAY_TABLE_SCALE_1MS            0x00000002 /* RW--V */
#define LW_EC_INFO_PWR_SEQ_DELAY_TABLE_SCALE_10MS           0x00000003 /* RW--V */
#define LW_EC_INFO_PWR_SEQ_DELAY_TABLE_NUM_STEPS                   7:2 /* RWIVF */
#define LW_EC_INFO_PWR_SEQ_DELAY_TABLE_NUM_STEPS_0          0x00000000 /* RWI-V */

/******************************************************************************

PWR_SEQ_INST - Power sequencer instruction table
    This is a register array that contains all the program couner values
    for the power sequencer. Instructions are packed in opcode/data format.
    The data field is 6 bits, and may take on different values depending on
    the OPCODE

OPCODE:
    UPDATE_OUTPUT - Update an output value, and optionally trigger it. All
                    sequencer values are double buffered so that multiple
                    pending outputs can be triggered at once.
    DELAY_TIME    - Wait for a time delay. Look up in the delay table
    HALT          - Stop exelwtion and report back to the Master FSM

DATA:
    UPDATE_OUTPUT:
        SIGNAL_IDX - Signal index to change
        SIGNAL_VAL - Value to set SIGNAL_IDX. This updates the ARMED state
        TRIG       - Trigger all outputs. Updates ARMED state to ACTIVE state
    DELAY_TIME:
        DELAY_IDX - Index to the DELAY_TABLE representing a time delay
******************************************************************************/

#define LW_EC_INFO_PWR_SEQ_INST(i)             (0x00000170 + (i))   /* RW-1A */
#define LW_EC_INFO_PWR_SEQ_INST__SIZE_1                       256   /*       */
#define LW_EC_INFO_PWR_SEQ_INST_DATA                          5:0   /* RWIVF */
#define LW_EC_INFO_PWR_SEQ_INST_DATA_INIT             0x000000000   /* RWI-V */

//aliased fields
#define LW_EC_INFO_PWR_SEQ_INST_SIGNAL_IDX                         3:0   /* RW-VF */
#define LW_EC_INFO_PWR_SEQ_INST_SIGNAL_IDX_GPU_EVENT        0x00000000   /* RW--V */
#define LW_EC_INFO_PWR_SEQ_INST_SIGNAL_IDX_VR_EN_3V3        0x00000001   /* RW--V */
#define LW_EC_INFO_PWR_SEQ_INST_SIGNAL_IDX_VR_EN_LWVDD      0x00000002   /* RW--V */
#define LW_EC_INFO_PWR_SEQ_INST_SIGNAL_IDX_VR_EN_VRAM_TOTAL 0x00000003   /* RW--V */
#define LW_EC_INFO_PWR_SEQ_INST_SIGNAL_IDX_VR_EN_GPU_FBIO   0x00000004   /* RW--V */
#define LW_EC_INFO_PWR_SEQ_INST_SIGNAL_IDX_VR_EN_1V         0x00000005   /* RW--V */
#define LW_EC_INFO_PWR_SEQ_INST_SIGNAL_IDX_EC_GPU_RST       0x00000006   /* RW--V */
#define LW_EC_INFO_PWR_SEQ_INST_SIGNAL_IDX_PGOOD            0x00000007   /* RW--V */
#define LW_EC_INFO_PWR_SEQ_INST_SIGNAL_IDX_PWR_ALERT        0x00000008   /* RW--V */
#define LW_EC_INFO_PWR_SEQ_INST_SIGNAL_IDX_MISC0            0x00000009   /* RW--V */
#define LW_EC_INFO_PWR_SEQ_INST_SIGNAL_IDX_MISC1            0x0000000a   /* RW--V */
#define LW_EC_INFO_PWR_SEQ_INST_SIGNAL_VAL                         4:4   /* RW-VF */
#define LW_EC_INFO_PWR_SEQ_INST_SIGNAL_VAL_DEASSERT         0x00000000   /* RW--V */
#define LW_EC_INFO_PWR_SEQ_INST_SIGNAL_VAL_ASSERT           0x00000001   /* RW--V */
#define LW_EC_INFO_PWR_SEQ_INST_TRIG                               5:5   /* RW-VF */
#define LW_EC_INFO_PWR_SEQ_INST_TRIG_NO                     0x00000000   /* RW--V */
#define LW_EC_INFO_PWR_SEQ_INST_TRIG_YES                    0x00000001   /* RW--V */
#define LW_EC_INFO_PWR_SEQ_INST_DELAY_IDX                          3:0   /* RW-VF */

//end of aliased fields

#define LW_EC_INFO_PWR_SEQ_INST_OPCODE                        7:6   /* RWIVF */
#define LW_EC_INFO_PWR_SEQ_INST_OPCODE_HALT           0x000000000   /* RWI-V */
#define LW_EC_INFO_PWR_SEQ_INST_OPCODE_UPDATE_OUTPUT  0x000000001   /* RW--V */
#define LW_EC_INFO_PWR_SEQ_INST_OPCODE_DELAY_TIME     0x000000002   /* RW--V */

/*
Header Goals:

1. Track EC versioning and capabilities
2. Track FSM status
3. Trigger the Master FSM to run
4. Configure IOs and read IO config/status
5. Configure the power sequencer instructions for arbitrary power sequences during cold boot and GC6
6. Update EEPROM with user data
7. Change FSM Modes
8. Error status (timeouts etc)

*/
#endif // _ec_info_h__included_
