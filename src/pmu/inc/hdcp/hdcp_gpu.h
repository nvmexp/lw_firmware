/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2008-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   hdcp_gpu.h
 * @brief  Header for CPU REG re-implementation into a base and offset form.
 *
 */

#ifndef HDCP_GPU_H
#define HDCP_GPU_H

//
// This header re-implements the GT216 manual header into a
// base and offset form.  There are two sets of HDCP downstream
// downstream registers: (1) DP - SOR_DP_HDCP..., and
// (2) TMDS - SOR_TMDS_HDCP....  There is one slight difference
// between the two sets of registers that prevents from implementing
// another set of registers to combine the two.  However, this
// difference does not prevent the implementation of a base and
// offset form.  This new form will help consolidate implementation
// to one set of routines as opposed to two set required for the
// original implementation.
//

#define HDCP_SOR_DP_BASE              (0x0061C200)
#define HDCP_SOR_TMDS_BASE            (0x0061C300)


//
//! GPU HDCP Register Offsets
//

//
//! Lower set for DP and TMDS monitors
//
#define HDCP_AN_MSB(i)                (0x50 + i*2048)

#define HDCP_AN_LSB(i)                (0x54 + i*2048)

#define HDCP_AKSV_MSB(i)              (0x60 + i*2048)
#define HDCP_AKSV_MSB_VALUE             (0x000000FF)

#define HDCP_AKSV_LSB(i)              (0x64 + i*2048)

#define HDCP_BKSV_MSB(i)              (0x68 + i*2048)
#define HDCP_BKSV_MSB_REPEATER          (0x80000000)

#define HDCP_BKSV_LSB(i)              (0x6C + i*2048)

#define HDCP_CTRL(i)                  (0x80 + i*2048)
#define HDCP_CTRL_RUN                   (0x00000001)
#define HDCP_CTRL_CRYPT                 (0x00000002)
#define HDCP_CTRL_AN                    (0x00000100)
#define HDCP_CTRL_R0                    (0x00000200)
#define HDCP_CTRL_SROM_EN               (0x00001000)
#define HDCP_CTRL_SROM_ERR              (0x00002000)
#define HDCP_CTRL_UPSTREAM              (0x00008000)

#define HDCP_RI(i)                    (0x9C + i*2048)
#define HDCP_RI_VALUE                   (0x0000FFFF)

#define HDCP_CYA(i)                   (0xFC + i*2048)  //! WHAT IS THIS FOR?  TSTDEBUG

//
//! Higher set for TMDS-dual link devices only.  Secondary Link
//

#define HDCP_AN_S_MSB(i)               (0xB4 + i*2048)

#define HDCP_AN_S_LSB(i)               (0xB8 + i*2048)

#define HDCP_BKSV_S_MSB(i)             (0xBC + i*2048)
#define HDCP_BKSV_S_MSB_REPEATER         (0x80000000)

#define HDCP_BKSV_S_LSB(i)             (0xC0 + i*2048)

#define HDCP_RI_S(i)                   (0xC4 + i*2048)
#define HDCP_RI_S_VALUE                  (0x0000FFFF)


#endif // HDCP_GPU_H
