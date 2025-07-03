/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef __ga102_dev_pwr_pri_addendum_h__
#define __ga102_dev_pwr_pri_addendum_h__

/*!
 * Defines the PMU Message Queue sizes
 */
#define  LW_PPWR_PMU_MSGQ_HEAD__SIZE_1      (1)
#define  LW_PPWR_PMU_MSGQ_TAIL__SIZE_1      (1)

#ifndef LW_PPWR_FALCON_ICD_IDX_RSTAT__SIZE_1
#define LW_PPWR_FALCON_ICD_IDX_RSTAT__SIZE_1    (6)
#endif

//
// These bits are set by the IFR (Init From ROM) PMU code to
// indicate its state
//
#define LW_PPWR_FALCON_MAILBOX0_IFR_PMU_STARTED                    8:8
#define LW_PPWR_FALCON_MAILBOX0_IFR_PMU_STARTED_ON                 0x00000001
#define LW_PPWR_FALCON_MAILBOX0_IFR_PMU_STARTED_OFF                0x00000000
#define LW_PPWR_FALCON_MAILBOX0_IFR_PMU_READY                      9:9
#define LW_PPWR_FALCON_MAILBOX0_IFR_PMU_READY_ON                   0x00000001
#define LW_PPWR_FALCON_MAILBOX0_IFR_PMU_READY_OFF                  0x00000000

//
// Used as a per-port mutex where each bit of the mailbox register corresponds 
// to a physical I2C port.
//
#define LW_PPWR_PMU_MAILBOX1                                       LW_PPWR_PMU_MAILBOX(1)
#define LW_PPWR_PMU_MAILBOX1_I2C_MUTEX_PORT(i)                        (i):(i) /* RWIVF */
#define LW_PPWR_PMU_MAILBOX1_I2C_MUTEX_PORT_FREE                   0x00000000 /* RWI-V */
#define LW_PPWR_PMU_MAILBOX1_I2C_MUTEX_PORT_ACQUIRED               0x00000001 /* RW--V */

#define LW_PMU_MUTEX_DEFINES \
    LW_MUTEX_ID_ILWALID   ,  \
    LW_MUTEX_ID_GPUSER    ,  \
    LW_MUTEX_ID_QUEUE_BIOS,  \
    LW_MUTEX_ID_QUEUE_SMI ,  \
    LW_MUTEX_ID_GPMUTEX   ,  \
    LW_MUTEX_ID_ILWALID   ,  \
    LW_MUTEX_ID_RMLOCK    ,  \
    LW_MUTEX_ID_MSGBOX    ,  \
    LW_MUTEX_ID_FIFO      ,  \
    LW_MUTEX_ID_ILWALID   ,  \
    LW_MUTEX_ID_GR        ,  \
    LW_MUTEX_ID_CLK       ,  \
    LW_MUTEX_ID_GPIO      ,  \
    LW_MUTEX_ID_ILWALID   ,  \
    LW_MUTEX_ID_ILWALID   ,  \
    LW_MUTEX_ID_ILWALID   ,

#endif // __ga102_dev_pwr_pri_addendum_h__
