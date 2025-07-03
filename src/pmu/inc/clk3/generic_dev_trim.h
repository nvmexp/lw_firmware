/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2003-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */


/*
 * @file
 *
 * @details     The purpose of this file is to create generic definitions for
 *              the various frequency source hardware object (PLLs, OSMs, etc.)
 *              that have common field/value definition.  This file should not
 *              contain register definitions.
 *
 *              #incldue this file instead of dev_trim.h (and dev_trim_addendum.h).
 *              (This differs from the RM version of generic_dev_trim.h.)
 *
 *              You may #include this file more than once.  Each #define is
 *              conditional on prior definition so avoid duplication.  If two
 *              (or more) chips use conflicting definitions, then mcheck should
 *              be able to detect it.
 *
 *              For most chips, the GPC registers make a good baseline.  If your
 *              chip does not implement these registers, or if it uses additional
 *              field definitions, please add it to this file.
 *
 *              Please do not reuse a generic name for a field with a different contract.
 *              For example, the value used in the LW_PTRIM_SYS_CLK_LDIV_ONESRC0DIV
 *              field is 2*d-2 where d is the divider value (possibly fractional).
 *              If the hardware folks come up with a different formula (i.e. contract),
 *              then they and we should have a different name for the field so that
 *              mcheck will flag any misuse.
 */

#ifndef CLK_SD_CHECK        // See pmu_sw/prod_app/clk/clk3/clksdcheck.c
#include "lwmisc.h"
#include "dbgprintf.h"
#endif

#include "dev_trim.h"
#include "dev_trim_addendum.h"
#include "dev_fbpa.h"
#include "dev_fbpa_addendum.h"


/*******************************************************************************
    PLLs
*******************************************************************************/

// _INIT values for all fields.
#ifndef LW_PTRIM_SYS_PLL_CFG_INIT
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG
#define LW_PTRIM_SYS_PLL_CFG_INIT                      (LW_PTRIM_SYS_GPCPLL_CFG_ENABLE_INIT       | \
                                                        LW_PTRIM_SYS_GPCPLL_CFG_IDDQ_INIT         | \
                                                        LW_PTRIM_SYS_GPCPLL_CFG_SYNC_MODE_INIT    | \
                                                        LW_PTRIM_SYS_GPCPLL_CFG_ENB_LCKDET_INIT   | \
                                                        LW_PTRIM_SYS_GPCPLL_CFG_LOCK_OVERRIDE_INIT  )
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_ENABLE
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_ENABLE
#define LW_PTRIM_SYS_PLL_CFG_ENABLE                     LW_PTRIM_SYS_GPCPLL_CFG_ENABLE
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG_ENABLE)
#define LW_PTRIM_SYS_PLL_CFG_ENABLE                     LW_PTRIM_GPC_BCAST_GPCPLL_CFG_ENABLE
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_ENABLE_NO
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_ENABLE_NO
#define LW_PTRIM_SYS_PLL_CFG_ENABLE_NO                  LW_PTRIM_SYS_GPCPLL_CFG_ENABLE_NO
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG_ENABLE_NO)
#define LW_PTRIM_SYS_PLL_CFG_ENABLE_NO                  LW_PTRIM_GPC_BCAST_GPCPLL_CFG_ENABLE_NO
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_ENABLE_YES
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_ENABLE_YES
#define LW_PTRIM_SYS_PLL_CFG_ENABLE_YES                 LW_PTRIM_SYS_GPCPLL_CFG_ENABLE_YES
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG_ENABLE_YES)
#define LW_PTRIM_SYS_PLL_CFG_ENABLE_YES                 LW_PTRIM_GPC_BCAST_GPCPLL_CFG_ENABLE_YES
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_IDDQ
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_IDDQ
#define LW_PTRIM_SYS_PLL_CFG_IDDQ                       LW_PTRIM_SYS_GPCPLL_CFG_IDDQ
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG_IDDQ)
#define LW_PTRIM_SYS_PLL_CFG_IDDQ                       LW_PTRIM_GPC_BCAST_GPCPLL_CFG_IDDQ
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_IDDQ_POWER
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_IDDQ_POWER
#define LW_PTRIM_SYS_PLL_CFG_IDDQ_POWER                 LW_PTRIM_SYS_GPCPLL_CFG_IDDQ_POWER
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG_IDDQ_POWER)
#define LW_PTRIM_SYS_PLL_CFG_IDDQ_POWER                 LW_PTRIM_GPC_BCAST_GPCPLL_CFG_IDDQ_POWER
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_IDDQ_POWER_ON
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_IDDQ_POWER_ON
#define LW_PTRIM_SYS_PLL_CFG_IDDQ_POWER_ON              LW_PTRIM_SYS_GPCPLL_CFG_IDDQ_POWER_ON
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG_IDDQ_POWER_ON)
#define LW_PTRIM_SYS_PLL_CFG_IDDQ_POWER_ON              LW_PTRIM_GPC_BCAST_GPCPLL_CFG_IDDQ_POWER_ON
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_IDDQ_POWER_OFF
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_IDDQ_POWER_OFF
#define LW_PTRIM_SYS_PLL_CFG_IDDQ_POWER_OFF             LW_PTRIM_SYS_GPCPLL_CFG_IDDQ_POWER_OFF
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG_IDDQ_POWER_OFF)
#define LW_PTRIM_SYS_PLL_CFG_IDDQ_POWER_OFF             LW_PTRIM_GPC_BCAST_GPCPLL_CFG_IDDQ_POWER_OFF
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_SYNC_MODE
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_SYNC_MODE
#define LW_PTRIM_SYS_PLL_CFG_SYNC_MODE                  LW_PTRIM_SYS_GPCPLL_CFG_SYNC_MODE
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG_SYNC_MODE)
#define LW_PTRIM_SYS_PLL_CFG_SYNC_MODE                  LW_PTRIM_GPC_BCAST_GPCPLL_CFG_SYNC_MODE
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_SYNC_MODE_DISABLE
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_SYNC_MODE_DISABLE
#define LW_PTRIM_SYS_PLL_CFG_SYNC_MODE_DISABLE          LW_PTRIM_SYS_GPCPLL_CFG_SYNC_MODE_DISABLE
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG_SYNC_MODE_DISABLE)
#define LW_PTRIM_SYS_PLL_CFG_SYNC_MODE_DISABLE          LW_PTRIM_GPC_BCAST_GPCPLL_CFG_SYNC_MODE_DISABLE
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_SYNC_MODE_ENABLE
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_SYNC_MODE_ENABLE
#define LW_PTRIM_SYS_PLL_CFG_SYNC_MODE_ENABLE           LW_PTRIM_SYS_GPCPLL_CFG_SYNC_MODE_ENABLE
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG_SYNC_MODE_ENABLE)
#define LW_PTRIM_SYS_PLL_CFG_SYNC_MODE_ENABLE           LW_PTRIM_GPC_BCAST_GPCPLL_CFG_SYNC_MODE_ENABLE
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_ENB_LCKDET
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_ENB_LCKDET
#define LW_PTRIM_SYS_PLL_CFG_ENB_LCKDET                 LW_PTRIM_SYS_GPCPLL_CFG_ENB_LCKDET
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG_ENB_LCKDET)
#define LW_PTRIM_SYS_PLL_CFG_ENB_LCKDET                 LW_PTRIM_GPC_BCAST_GPCPLL_CFG_ENB_LCKDET
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_ENB_LCKDET_POWER_ON
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_ENB_LCKDET_POWER_ON
#define LW_PTRIM_SYS_PLL_CFG_ENB_LCKDET_POWER_ON        LW_PTRIM_SYS_GPCPLL_CFG_ENB_LCKDET_POWER_ON
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG_ENB_LCKDET_POWER_ON)
#define LW_PTRIM_SYS_PLL_CFG_ENB_LCKDET_POWER_ON        LW_PTRIM_GPC_BCAST_GPCPLL_CFG_ENB_LCKDET_POWER_ON
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_ENB_LCKDET_POWER_OFF
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_ENB_LCKDET_POWER_OFF
#define LW_PTRIM_SYS_PLL_CFG_ENB_LCKDET_POWER_OFF       LW_PTRIM_SYS_GPCPLL_CFG_ENB_LCKDET_POWER_OFF
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG_ENB_LCKDET_POWER_OFF)
#define LW_PTRIM_SYS_PLL_CFG_ENB_LCKDET_POWER_OFF       LW_PTRIM_GPC_BCAST_GPCPLL_CFG_ENB_LCKDET_POWER_OFF
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_EN_LCKDET
#if                                             defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG_EN_LCKDET)
#define LW_PTRIM_SYS_PLL_CFG_EN_LCKDET                  LW_PTRIM_GPC_BCAST_GPCPLL_CFG_EN_LCKDET
#elif                                           defined(LW_PTRIM_SYS_PLL_CFG_ENB_LCKDET)
#define LW_PTRIM_SYS_PLL_CFG_EN_LCKDET                  LW_PTRIM_SYS_PLL_CFG_ENB_LCKDET
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_EN_LCKDET_POWER_ON
#if                                             defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG_EN_LCKDET_POWER_ON)
#define LW_PTRIM_SYS_PLL_CFG_EN_LCKDET_POWER_ON         LW_PTRIM_GPC_BCAST_GPCPLL_CFG_EN_LCKDET_POWER_ON
#elif                                           defined(LW_PTRIM_SYS_PLL_CFG_ENB_LCKDET_POWER_ON)
#define LW_PTRIM_SYS_PLL_CFG_EN_LCKDET_POWER_ON       !(LW_PTRIM_SYS_PLL_CFG_ENB_LCKDET_POWER_ON)
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_EN_LCKDET_POWER_OFF
#if                                             defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG_EN_LCKDET_POWER_OFF)
#define LW_PTRIM_SYS_PLL_CFG_EN_LCKDET_POWER_OFF        LW_PTRIM_GPC_BCAST_GPCPLL_CFG_EN_LCKDET_POWER_OFF
#elif                                           defined(LW_PTRIM_SYS_PLL_CFG_ENB_LCKDET_POWER_OFF)
#define LW_PTRIM_SYS_PLL_CFG_EN_LCKDET_POWER_OFF      !(LW_PTRIM_SYS_PLL_CFG_ENB_LCKDET_POWER_OFF)
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_LOCK_OVERRIDE
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_LOCK_OVERRIDE
#define LW_PTRIM_SYS_PLL_CFG_LOCK_OVERRIDE              LW_PTRIM_SYS_GPCPLL_CFG_LOCK_OVERRIDE
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG_LOCK_OVERRIDE)
#define LW_PTRIM_SYS_PLL_CFG_LOCK_OVERRIDE              LW_PTRIM_GPC_BCAST_GPCPLL_CFG_LOCK_OVERRIDE
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_LOCK_OVERRIDE_DISABLE
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_LOCK_OVERRIDE_DISABLE
#define LW_PTRIM_SYS_PLL_CFG_LOCK_OVERRIDE_DISABLE      LW_PTRIM_SYS_GPCPLL_CFG_LOCK_OVERRIDE_DISABLE
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG_LOCK_OVERRIDE_DISABLE)
#define LW_PTRIM_SYS_PLL_CFG_LOCK_OVERRIDE_DISABLE      LW_PTRIM_GPC_BCAST_GPCPLL_CFG_LOCK_OVERRIDE_DISABLE
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_LOCK_OVERRIDE_ENABLE
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_LOCK_OVERRIDE_ENABLE
#define LW_PTRIM_SYS_PLL_CFG_LOCK_OVERRIDE_ENABLE       LW_PTRIM_SYS_GPCPLL_CFG_LOCK_OVERRIDE_ENABLE
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG_LOCK_OVERRIDE_ENABLE)
#define LW_PTRIM_SYS_PLL_CFG_LOCK_OVERRIDE_ENABLE       LW_PTRIM_GPC_BCAST_GPCPLL_CFG_LOCK_OVERRIDE_ENABLE
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_PLL_READY
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_PLL_READY
#define LW_PTRIM_SYS_PLL_CFG_PLL_READY                  LW_PTRIM_SYS_GPCPLL_CFG_PLL_READY
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG_PLL_READY)
#define LW_PTRIM_SYS_PLL_CFG_PLL_READY                  LW_PTRIM_GPC_BCAST_GPCPLL_CFG_PLL_READY
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_PLL_READY_FALSE
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_PLL_READY_FALSE
#define LW_PTRIM_SYS_PLL_CFG_PLL_READY_FALSE            LW_PTRIM_SYS_GPCPLL_CFG_PLL_READY_FALSE
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG_PLL_READY_FALSE)
#define LW_PTRIM_SYS_PLL_CFG_PLL_READY_FALSE            LW_PTRIM_GPC_BCAST_GPCPLL_CFG_PLL_READY_FALSE
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_PLL_READY_TRUE
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_PLL_READY_TRUE
#define LW_PTRIM_SYS_PLL_CFG_PLL_READY_TRUE             LW_PTRIM_SYS_GPCPLL_CFG_PLL_READY_TRUE
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG_PLL_READY_TRUE)
#define LW_PTRIM_SYS_PLL_CFG_PLL_READY_TRUE             LW_PTRIM_GPC_BCAST_GPCPLL_CFG_PLL_READY_TRUE
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_PLL_LOCK
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_PLL_LOCK
#define LW_PTRIM_SYS_PLL_CFG_PLL_LOCK                   LW_PTRIM_SYS_GPCPLL_CFG_PLL_LOCK
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG_PLL_LOCK)
#define LW_PTRIM_SYS_PLL_CFG_PLL_LOCK                   LW_PTRIM_GPC_BCAST_GPCPLL_CFG_PLL_LOCK
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_PLL_LOCK_FALSE
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_PLL_LOCK_FALSE
#define LW_PTRIM_SYS_PLL_CFG_PLL_LOCK_FALSE             LW_PTRIM_SYS_GPCPLL_CFG_PLL_LOCK_FALSE
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG_PLL_LOCK_FALSE)
#define LW_PTRIM_SYS_PLL_CFG_PLL_LOCK_FALSE             LW_PTRIM_GPC_BCAST_GPCPLL_CFG_PLL_LOCK_FALSE
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_PLL_LOCK_TRUE
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_PLL_LOCK_TRUE
#define LW_PTRIM_SYS_PLL_CFG_PLL_LOCK_TRUE              LW_PTRIM_SYS_GPCPLL_CFG_PLL_LOCK_TRUE
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG_PLL_LOCK_TRUE)
#define LW_PTRIM_SYS_PLL_CFG_PLL_LOCK_TRUE              LW_PTRIM_GPC_BCAST_GPCPLL_CFG_PLL_LOCK_TRUE
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_BYPASSPLL
#ifdef                                                  LW_PVTRIM_SYS_DISPPLL_CFG_BYPASSPLL
#define LW_PTRIM_SYS_PLL_CFG_BYPASSPLL                  LW_PVTRIM_SYS_DISPPLL_CFG_BYPASSPLL
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_BYPASSPLL_NO
#ifdef                                                  LW_PVTRIM_SYS_DISPPLL_CFG_BYPASSPLL_NO
#define LW_PTRIM_SYS_PLL_CFG_BYPASSPLL_NO               LW_PVTRIM_SYS_DISPPLL_CFG_BYPASSPLL_NO
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_BYPASSPLL_YES
#ifdef                                                  LW_PVTRIM_SYS_DISPPLL_CFG_BYPASSPLL_YES
#define LW_PTRIM_SYS_PLL_CFG_BYPASSPLL_YES              LW_PVTRIM_SYS_DISPPLL_CFG_BYPASSPLL_YES
#endif
#endif

// Combine _READY and _LOCK in a way not done in the manuals.
#ifndef LW_PTRIM_SYS_PLL_CFG_PLL_STATUS
#if  defined(LW_PTRIM_SYS_PLL_CFG_PLL_READY) && defined(LW_PTRIM_SYS_PLL_CFG_PLL_LOCK)
ct_assert(DRF_SIZE(LW_PTRIM_SYS_PLL_CFG_PLL_READY) == 1);
ct_assert(DRF_SIZE(LW_PTRIM_SYS_PLL_CFG_PLL_LOCK) == 1);
ct_assert(DRF_EXTENT(LW_PTRIM_SYS_PLL_CFG_PLL_READY)  + 1 == DRF_BASE(LW_PTRIM_SYS_PLL_CFG_PLL_LOCK));
#define LW_PTRIM_SYS_PLL_CFG_PLL_STATUS                  DRF_EXTENT(LW_PTRIM_SYS_PLL_CFG_PLL_LOCK):DRF_BASE(LW_PTRIM_SYS_PLL_CFG_PLL_READY)
#define LW_PTRIM_SYS_PLL_CFG_PLL_STATUS_NOT_READY       (LW_PTRIM_SYS_GPCPLL_CFG_PLL_READY_FALSE | LW_PTRIM_SYS_GPCPLL_CFG_PLL_LOCK_FALSE)
#define LW_PTRIM_SYS_PLL_CFG_PLL_STATUS_READY           (LW_PTRIM_SYS_GPCPLL_CFG_PLL_READY_TRUE  | LW_PTRIM_SYS_GPCPLL_CFG_PLL_LOCK_FALSE)
#define LW_PTRIM_SYS_PLL_CFG_PLL_STATUS_LOCKED          (LW_PTRIM_SYS_GPCPLL_CFG_PLL_READY_TRUE  | LW_PTRIM_SYS_GPCPLL_CFG_PLL_LOCK_TRUE)
#endif  // _READY & _LOCK fields exist
#endif  // Pseudo-field has not yet been defined


#ifndef LW_PTRIM_SYS_PLL_CFG_PLL_SSA
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_PLL_SSA
#define LW_PTRIM_SYS_PLL_CFG_PLL_SSA                    LW_PTRIM_SYS_GPCPLL_CFG_PLL_SSA
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_PLL_SSA_FALSE
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_PLL_SSA_FALSE
#define LW_PTRIM_SYS_PLL_CFG_PLL_SSA_FALSE              LW_PTRIM_SYS_GPCPLL_CFG_PLL_SSA_FALSE
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_PLL_SSA_TRUE
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_PLL_SSA_TRUE
#define LW_PTRIM_SYS_PLL_CFG_PLL_SSA_TRUE               LW_PTRIM_SYS_GPCPLL_CFG_PLL_SSA_TRUE
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_PLL_SSD
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_PLL_SSD
#define LW_PTRIM_SYS_PLL_CFG_PLL_SSD                    LW_PTRIM_SYS_GPCPLL_CFG_PLL_SSD
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_PLL_SSD_FALSE
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_PLL_SSD_FALSE
#define LW_PTRIM_SYS_PLL_CFG_PLL_SSD_FALSE              LW_PTRIM_SYS_GPCPLL_CFG_PLL_SSD_FALSE
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_PLL_SSD_TRUE
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_PLL_SSD_TRUE
#define LW_PTRIM_SYS_PLL_CFG_PLL_SSD_TRUE               LW_PTRIM_SYS_GPCPLL_CFG_PLL_SSD_TRUE
#endif
#endif

// Combine analog and digital spread spectrum in a way not done in the manuals.
#ifndef LW_PTRIM_SYS_PLL_CFG_PLL_SS
#if     defined(LW_PTRIM_SYS_PLL_CFG_PLL_SSA) && defined(LW_PTRIM_SYS_PLL_CFG_PLL_SSD)
ct_assert(DRF_SIZE(LW_PTRIM_SYS_PLL_CFG_PLL_SSA) == 1);
ct_assert(DRF_SIZE(LW_PTRIM_SYS_PLL_CFG_PLL_SSD) == 1);
ct_assert(DRF_EXTENT(LW_PTRIM_SYS_PLL_CFG_PLL_SSA)  + 1 == DRF_BASE(LW_PTRIM_SYS_PLL_CFG_PLL_SSD));
#define LW_PTRIM_SYS_PLL_CFG_PLL_SS                     DRF_EXTENT(LW_PTRIM_SYS_PLL_CFG_PLL_SSD):DRF_BASE(LW_PTRIM_SYS_PLL_CFG_PLL_SSA)
#define LW_PTRIM_SYS_PLL_CFG_PLL_SS_NONE                (LW_PTRIM_SYS_GPCPLL_CFG_PLL_SSA_FALSE | (LW_PTRIM_SYS_GPCPLL_CFG_PLL_SSD_FALSE << 1))
#define LW_PTRIM_SYS_PLL_CFG_PLL_SS_ALANOG              (LW_PTRIM_SYS_GPCPLL_CFG_PLL_SSA_TRUE  | (LW_PTRIM_SYS_GPCPLL_CFG_PLL_SSD_FALSE << 1))
#define LW_PTRIM_SYS_PLL_CFG_PLL_SS_DIGITAL             (LW_PTRIM_SYS_GPCPLL_CFG_PLL_SSA_FALSE | (LW_PTRIM_SYS_GPCPLL_CFG_PLL_SSD_TRUE  << 1))
#define LW_PTRIM_SYS_PLL_CFG_PLL_SS_BOTH                (LW_PTRIM_SYS_GPCPLL_CFG_PLL_SSA_TRUE  | (LW_PTRIM_SYS_GPCPLL_CFG_PLL_SSD_TRUE  << 1))
#endif      // Analog and Digital spread spectrum fields exist
#endif      // Pseudo-field has not yet been defined


#ifndef LW_PTRIM_SYS_PLL_CFG_PLL_CML
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_PLL_CML
#define LW_PTRIM_SYS_PLL_CFG_PLL_CML                    LW_PTRIM_SYS_GPCPLL_CFG_PLL_CML
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_PLL_CML_FALSE
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_PLL_CML_FALSE
#define LW_PTRIM_SYS_PLL_CFG_PLL_CML_FALSE              LW_PTRIM_SYS_GPCPLL_CFG_PLL_CML_FALSE
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_PLL_CML_TRUE
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_PLL_CML_TRUE
#define LW_PTRIM_SYS_PLL_CFG_PLL_CML_TRUE               LW_PTRIM_SYS_GPCPLL_CFG_PLL_CML_TRUE
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_PLL_4PHCML
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_PLL_4PHCML
#define LW_PTRIM_SYS_PLL_CFG_PLL_4PHCML                 LW_PTRIM_SYS_GPCPLL_CFG_PLL_4PHCML
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_PLL_4PHCML_FALSE
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_PLL_4PHCML_FALSE
#define LW_PTRIM_SYS_PLL_CFG_PLL_4PHCML_FALSE           LW_PTRIM_SYS_GPCPLL_CFG_PLL_4PHCML_FALSE
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_PLL_4PHCML_TRUE
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_PLL_4PHCML_TRUE
#define LW_PTRIM_SYS_PLL_CFG_PLL_4PHCML_TRUE            LW_PTRIM_SYS_GPCPLL_CFG_PLL_4PHCML_TRUE
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_PLL_DLL
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_PLL_DLL
#define LW_PTRIM_SYS_PLL_CFG_PLL_DLL                    LW_PTRIM_SYS_GPCPLL_CFG_PLL_DLL
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_PLL_DLL_FALSE
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_PLL_DLL_FALSE
#define LW_PTRIM_SYS_PLL_CFG_PLL_DLL_FALSE              LW_PTRIM_SYS_GPCPLL_CFG_PLL_DLL_FALSE
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG_PLL_DLL_TRUE
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG_PLL_DLL_TRUE
#define LW_PTRIM_SYS_PLL_CFG_PLL_DLL_TRUE               LW_PTRIM_SYS_GPCPLL_CFG_PLL_DLL_TRUE
#endif
#endif

// _INIT values for all fields, except that invalid zeroes are replaced with ones.
#ifndef LW_PTRIM_SYS_PLL_COEFF_INIT
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_COEFF
#define LW_PTRIM_SYS_PLL_COEFF_INIT                   ((LW_PTRIM_SYS_GPCPLL_COEFF_MDIV_INIT?  LW_PTRIM_SYS_GPCPLL_COEFF_MDIV_INIT:  0x00000001) | \
                                                       (LW_PTRIM_SYS_GPCPLL_COEFF_NDIV_INIT?  LW_PTRIM_SYS_GPCPLL_COEFF_NDIV_INIT:  0x00000001) | \
                                                       (LW_PTRIM_SYS_GPCPLL_COEFF_PLDIV_INIT? LW_PTRIM_SYS_GPCPLL_COEFF_PLDIV_INIT: 0x00000001)   )
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_COEFF)
#define LW_PTRIM_SYS_PLL_COEFF_INIT                   ((LW_PTRIM_GPC_BCAST_GPCPLL_COEFF_MDIV_INIT?  LW_PTRIM_GPC_BCAST_GPCPLL_COEFF_MDIV_INIT:  0x00000001) | \
                                                       (LW_PTRIM_GPC_BCAST_GPCPLL_COEFF_NDIV_INIT?  LW_PTRIM_GPC_BCAST_GPCPLL_COEFF_NDIV_INIT:  0x00000001) | \
                                                       (LW_PTRIM_GPC_BCAST_GPCPLL_COEFF_PLDIV_INIT? LW_PTRIM_GPC_BCAST_GPCPLL_COEFF_PLDIV_INIT: 0x00000001)   )
#endif
#endif

// Both analog and hybrid PLLs
#ifndef LW_PTRIM_SYS_PLL_COEFF_MDIV
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_COEFF_MDIV
#define LW_PTRIM_SYS_PLL_COEFF_MDIV                     LW_PTRIM_SYS_GPCPLL_COEFF_MDIV
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_COEFF_MDIV)
#define LW_PTRIM_SYS_PLL_COEFF_MDIV                     LW_PTRIM_GPC_BCAST_GPCPLL_COEFF_MDIV
#endif
#endif

// Only analog PLLs -- Used exclusively in Turing and prior.
#ifndef LW_PTRIM_SYS_APLL_COEFF_NDIV
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_COEFF_NDIV
#define LW_PTRIM_SYS_APLL_COEFF_NDIV                    LW_PTRIM_SYS_GPCPLL_COEFF_NDIV
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_COEFF_NDIV)
#define LW_PTRIM_SYS_APLL_COEFF_NDIV                    LW_PTRIM_GPC_BCAST_GPCPLL_COEFF_NDIV
#endif
#endif

// Both analog and hybrid PLLs
#ifndef LW_PTRIM_SYS_PLL_COEFF_PLDIV
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_COEFF_PLDIV
#define LW_PTRIM_SYS_PLL_COEFF_PLDIV                    LW_PTRIM_SYS_GPCPLL_COEFF_PLDIV
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_COEFF_PLDIV)
#define LW_PTRIM_SYS_PLL_COEFF_PLDIV                    LW_PTRIM_GPC_BCAST_GPCPLL_COEFF_PLDIV
#endif
#endif

//
// Only read-only PLLs (e.g. SPPLLs, DEFROST/XTAL4x, etc.)
//
// SPPLLs are gone for displayless chips as of Hopper, so use DEFROST.
// As of Ampere, some of the defrost registers/fields have the legacy "XTAL4X"
// in the name.  The plan as of March 2020 is to change these names in the Hopper
// timeframe, so both "XTAL4X" and "DEFROST" are checked here at compile-time.
//
#ifndef LW_PTRIM_SYS_ROPLL_COEFF_MDIV
#if                                             defined(LW_PVTRIM_SYS_SPPLL0_COEFF_MDIV)
#define LW_PTRIM_SYS_ROPLL_COEFF_MDIV                   LW_PVTRIM_SYS_SPPLL0_COEFF_MDIV
#elif                                           defined(LW_PTRIM_SYS_XTAL4X_COEFF_MDIV)
#define LW_PTRIM_SYS_ROPLL_COEFF_MDIV                   LW_PTRIM_SYS_XTAL4X_COEFF_MDIV
#elif                                           defined(LW_PTRIM_SYS_DEFROST_COEFF_MDIV)
#define LW_PTRIM_SYS_ROPLL_COEFF_MDIV                   LW_PTRIM_SYS_DEFROST_COEFF_MDIV
#endif
#endif

// Only read-only PLLs (e.g. SPPLLs, DEFROST/XTAL4x, etc.)
#ifndef LW_PTRIM_SYS_ROPLL_COEFF_NDIV
#if                                             defined(LW_PVTRIM_SYS_SPPLL0_COEFF_NDIV)
#define LW_PTRIM_SYS_ROPLL_COEFF_NDIV                   LW_PVTRIM_SYS_SPPLL0_COEFF_NDIV
#elif                                           defined(LW_PTRIM_SYS_XTAL4X_COEFF_NDIV)
#define LW_PTRIM_SYS_ROPLL_COEFF_NDIV                   LW_PTRIM_SYS_XTAL4X_COEFF_NDIV
#elif                                           defined(LW_PTRIM_SYS_DEFROST_COEFF_NDIV)
#define LW_PTRIM_SYS_ROPLL_COEFF_NDIV                   LW_PTRIM_SYS_DEFROST_COEFF_NDIV
#endif
#endif

#ifndef LW_PVTRIM_SYS_PLL_CFG_EN_PLL_DYNRAMP
#ifdef                                                  LW_PVTRIM_SYS_DISPPLL_CFG_EN_PLL_DYNRAMP
#define LW_PVTRIM_SYS_PLL_CFG_EN_PLL_DYNRAMP            LW_PVTRIM_SYS_DISPPLL_CFG_EN_PLL_DYNRAMP
#endif
#endif

#ifndef LW_PVTRIM_SYS_PLL_CFG_EN_PLL_DYNRAMP_INIT
#ifdef                                                  LW_PVTRIM_SYS_DISPPLL_CFG_EN_PLL_DYNRAMP_INIT
#define LW_PVTRIM_SYS_PLL_CFG_EN_PLL_DYNRAMP_INIT       LW_PVTRIM_SYS_DISPPLL_CFG_EN_PLL_DYNRAMP_INIT
#endif
#endif

#ifndef LW_PVTRIM_SYS_PLL_CFG_EN_PLL_DYNRAMP_NO
#ifdef                                                  LW_PVTRIM_SYS_DISPPLL_CFG_EN_PLL_DYNRAMP_NO
#define LW_PVTRIM_SYS_PLL_CFG_EN_PLL_DYNRAMP_NO         LW_PVTRIM_SYS_DISPPLL_CFG_EN_PLL_DYNRAMP_NO
#endif
#endif

#ifndef LW_PVTRIM_SYS_PLL_CFG_EN_PLL_DYNRAMP_YES
#ifdef                                                  LW_PVTRIM_SYS_DISPPLL_CFG_EN_PLL_DYNRAMP_YES
#define LW_PVTRIM_SYS_PLL_CFG_EN_PLL_DYNRAMP_YES        LW_PVTRIM_SYS_DISPPLL_CFG_EN_PLL_DYNRAMP_YES
#endif
#endif

#ifndef LW_PVTRIM_SYS_PLL_CFG_DYNRAMP_DONE
#ifdef                                                  LW_PVTRIM_SYS_DISPPLL_CFG_DYNRAMP_DONE
#define LW_PVTRIM_SYS_PLL_CFG_DYNRAMP_DONE              LW_PVTRIM_SYS_DISPPLL_CFG_DYNRAMP_DONE
#endif
#endif

#ifndef LW_PVTRIM_SYS_PLL_CFG_DYNRAMP_DONE_FALSE
#ifdef                                                  LW_PVTRIM_SYS_DISPPLL_CFG_DYNRAMP_DONE_FALSE
#define LW_PVTRIM_SYS_PLL_CFG_DYNRAMP_DONE_FALSE        LW_PVTRIM_SYS_DISPPLL_CFG_DYNRAMP_DONE_FALSE
#endif
#endif

#ifndef LW_PVTRIM_SYS_PLL_CFG_DYNRAMP_DONE_TRUE
#ifdef                                                  LW_PVTRIM_SYS_DISPPLL_CFG_DYNRAMP_DONE_TRUE
#define LW_PVTRIM_SYS_PLL_CFG_DYNRAMP_DONE_TRUE         LW_PVTRIM_SYS_DISPPLL_CFG_DYNRAMP_DONE_TRUE
#endif
#endif

#ifndef LW_PVTRIM_SYS_PLL_DYN_SDM_DIN_NEW
#ifdef                                                  LW_PVTRIM_SYS_DISPPLL_DYN_SDM_DIN_NEW
#define LW_PVTRIM_SYS_PLL_DYN_SDM_DIN_NEW               LW_PVTRIM_SYS_DISPPLL_DYN_SDM_DIN_NEW
#endif
#endif

#ifndef LW_PVTRIM_SYS_PLL_DYN_SDM_DIN_NEW_INIT
#ifdef                                                  LW_PVTRIM_SYS_DISPPLL_DYN_SDM_DIN_NEW_INIT
#define LW_PVTRIM_SYS_PLL_DYN_SDM_DIN_NEW_INIT          LW_PVTRIM_SYS_DISPPLL_DYN_SDM_DIN_NEW_INIT
#endif
#endif

#ifndef LW_PVTRIM_SYS_PLL_COEFF_NDIV_NEW
#ifdef                                                  LW_PVTRIM_SYS_DISPPLL_COEFF_NDIV_NEW
#define LW_PVTRIM_SYS_PLL_COEFF_NDIV_NEW                LW_PVTRIM_SYS_DISPPLL_COEFF_NDIV_NEW
#endif
#endif

#ifndef LW_PFB_FBPA_COEFF_SEL_DIVBY2
#ifdef                                                  LW_PFB_FBPA_DRAMPLL_COEFF_SEL_DIVBY2
#define LW_PFB_FBPA_PLL_COEFF_SEL_DIVBY2                LW_PFB_FBPA_DRAMPLL_COEFF_SEL_DIVBY2
#endif
#endif

#ifndef LW_PFB_FBPA_COEFF_SEL_DIVBY2_ENABLE
#ifdef                                                  LW_PFB_FBPA_DRAMPLL_COEFF_SEL_DIVBY2_ENABLE
#define LW_PFB_FBPA_PLL_COEFF_SEL_DIVBY2_ENABLE         LW_PFB_FBPA_DRAMPLL_COEFF_SEL_DIVBY2_ENABLE
#endif
#endif

#ifndef LW_PFB_FBPA_COEFF_SEL_DIVBY2_DISABLE
#ifdef                                                  LW_PFB_FBPA_DRAMPLL_COEFF_SEL_DIVBY2_DISABLE
#define LW_PFB_FBPA_PLL_COEFF_SEL_DIVBY2_DISABLE        LW_PFB_FBPA_DRAMPLL_COEFF_SEL_DIVBY2_DISABLE
#endif
#endif

// Only hybrid PLLs -- Starting with GA10x, both analog and hybrid PLLs are used.
#ifndef LW_PTRIM_SYS_HPLL_CFG2_FRAC_STEP
#ifdef                                                  LW_PVTRIM_SYS_DISPPLL_CFG2_FRAC_STEP
#define LW_PTRIM_SYS_HPLL_CFG2_FRAC_STEP                LW_PVTRIM_SYS_DISPPLL_CFG2_FRAC_STEP
#endif
#endif

#ifndef LW_PTRIM_SYS_HPLL_CFG4_NDIV
#ifdef                                                  LW_PVTRIM_SYS_DISPPLL_CFG4_NDIV
#define LW_PTRIM_SYS_HPLL_CFG4_NDIV                     LW_PVTRIM_SYS_DISPPLL_CFG4_NDIV
#endif
#endif

#ifndef LW_PTRIM_SYS_HPLL_CFG4_NDIV_FRAC
#ifdef                                                  LW_PVTRIM_SYS_DISPPLL_CFG4_NDIV_FRAC
#define LW_PTRIM_SYS_HPLL_CFG4_NDIV_FRAC                LW_PVTRIM_SYS_DISPPLL_CFG4_NDIV_FRAC
#endif
#endif


//
// This multiplexer bypasses the VCO inside the HBMPLL (high-bandwidth memory).
//
// In Turing (and Volta), the PLL was placed in PTRIM, but has been moved to
// FBIO in Ampere to improve the quality of the clock signal.  As such, the
// register has been moved and these field definitions have changed accordingly.
//
// Since the semantics and behaviour has not changed, these aliases are useful
// for use inside Clocks 3.x logic.
//
// See bug 2425763
//
#ifndef LW_PFB_FBPA_HBMPLL_CFG_BYPASSPLL
#if                                             defined(LW_PTRIM_FBPA_BCAST_HBMPLL_CFG0_BYPASSPLL)      // Turing (and Volta)
#define LW_PFB_FBPA_HBMPLL_CFG_BYPASSPLL                LW_PTRIM_FBPA_BCAST_HBMPLL_CFG0_BYPASSPLL
#elif                                           defined(LW_PFB_FBPA_FBIO_HBMPLL_CFG_BYPASSPLL)          // Ampere and later
#define LW_PFB_FBPA_HBMPLL_CFG_BYPASSPLL                LW_PFB_FBPA_FBIO_HBMPLL_CFG_BYPASSPLL
#endif
#endif

// See bug 2425763
#ifndef LW_PFB_FBPA_HBMPLL_CFG_BYPASSPLL_VCO
#if                                             defined(LW_PTRIM_FBPA_BCAST_HBMPLL_CFG0_BYPASSPLL_VCO)  // Turing (and Volta)
#define LW_PFB_FBPA_HBMPLL_CFG_BYPASSPLL_VCO            LW_PTRIM_FBPA_BCAST_HBMPLL_CFG0_BYPASSPLL_VCO
#elif                                           defined(LW_PFB_FBPA_FBIO_HBMPLL_CFG_BYPASSPLL_VCO)      // Ampere and later
#define LW_PFB_FBPA_HBMPLL_CFG_BYPASSPLL_VCO            LW_PFB_FBPA_FBIO_HBMPLL_CFG_BYPASSPLL_VCO
#endif
#endif

// See bug 2425763
#ifndef LW_PFB_FBPA_HBMPLL_CFG_BYPASSPLL_BYPASSCLK
#if                                             defined(LW_PTRIM_FBPA_BCAST_HBMPLL_CFG0_BYPASSPLL_BYPASSCLK)    // Turing (and Volta)
#define LW_PFB_FBPA_HBMPLL_CFG_BYPASSPLL_BYPASSCLK      LW_PTRIM_FBPA_BCAST_HBMPLL_CFG0_BYPASSPLL_BYPASSCLK
#elif                                           defined(LW_PFB_FBPA_FBIO_HBMPLL_CFG_BYPASSPLL_BYPASSCLK)        // Ampere and later
#define LW_PFB_FBPA_HBMPLL_CFG_BYPASSPLL_BYPASSCLK      LW_PFB_FBPA_FBIO_HBMPLL_CFG_BYPASSPLL_BYPASSCLK
#endif
#endif


/*******************************************************************************
    Automatic Switch Dividers
*******************************************************************************/

//
// Automatic switch dividers are new as of GA10x and apply to both 2- and
// 4-input SWDIVs.  The defrost clock was called XTAL4X before Hopper, even
// though it was not 4x the xtal frequency.
//

// Hopper and later: Make sure dev_trim has the same value for both applicable domains.
#ifdef LW_PTRIM_SYS_DEFROST_UNIV_SEC_CLK_CTRL_CLOCK_SEL_OVERRIDE
#ifdef LW_PTRIM_SYS_PWRCLK_CTRL_CLOCK_SEL_OVERRIDE
ct_assert(DRF_EXTENT(LW_PTRIM_SYS_DEFROST_UNIV_SEC_CLK_CTRL_CLOCK_SEL_OVERRIDE) == DRF_EXTENT(LW_PTRIM_SYS_PWRCLK_CTRL_CLOCK_SEL_OVERRIDE));
ct_assert(  DRF_BASE(LW_PTRIM_SYS_DEFROST_UNIV_SEC_CLK_CTRL_CLOCK_SEL_OVERRIDE) ==   DRF_BASE(LW_PTRIM_SYS_PWRCLK_CTRL_CLOCK_SEL_OVERRIDE));
#endif
#endif

// GA10x: Make sure dev_trim has the same value for both applicable domains.
#ifdef LW_PTRIM_SYS_XTAL4X_UNIV_SEC_CLK_CTRL_CLOCK_SEL_OVERRIDE
#ifdef LW_PTRIM_SYS_PWRCLK_CTRL_CLOCK_SEL_OVERRIDE
ct_assert(DRF_EXTENT(LW_PTRIM_SYS_XTAL4X_UNIV_SEC_CLK_CTRL_CLOCK_SEL_OVERRIDE) == DRF_EXTENT(LW_PTRIM_SYS_PWRCLK_CTRL_CLOCK_SEL_OVERRIDE));
ct_assert(  DRF_BASE(LW_PTRIM_SYS_XTAL4X_UNIV_SEC_CLK_CTRL_CLOCK_SEL_OVERRIDE) ==   DRF_BASE(LW_PTRIM_SYS_PWRCLK_CTRL_CLOCK_SEL_OVERRIDE));
#endif
#endif

#ifndef LW_PTRIM_SYS_SWDIV_CLOCK_SEL_OVERRIDE
#if                                             defined(LW_PTRIM_SYS_DEFROST_UNIV_SEC_CLK_CTRL_CLOCK_SEL_OVERRIDE)          // Hopper and later
#define LW_PTRIM_SYS_SWDIV_CLOCK_SEL_OVERRIDE           LW_PTRIM_SYS_DEFROST_UNIV_SEC_CLK_CTRL_CLOCK_SEL_OVERRIDE
#elif                                           defined(LW_PTRIM_SYS_XTAL4X_UNIV_SEC_CLK_CTRL_CLOCK_SEL_OVERRIDE)           // GA10x
#define LW_PTRIM_SYS_SWDIV_CLOCK_SEL_OVERRIDE           LW_PTRIM_SYS_XTAL4X_UNIV_SEC_CLK_CTRL_CLOCK_SEL_OVERRIDE
#endif
#endif

#ifndef LW_PTRIM_SYS_SWDIV_CLOCK_SEL_OVERRIDE_ENABLE
#if                                             defined(LW_PTRIM_SYS_DEFROST_UNIV_SEC_CLK_CTRL_CLOCK_SEL_OVERRIDE_ENABLE)   // Hopper and later
#define LW_PTRIM_SYS_SWDIV_CLOCK_SEL_OVERRIDE_ENABLE    LW_PTRIM_SYS_DEFROST_UNIV_SEC_CLK_CTRL_CLOCK_SEL_OVERRIDE_ENABLE
#elif                                           defined(LW_PTRIM_SYS_XTAL4X_UNIV_SEC_CLK_CTRL_CLOCK_SEL_OVERRIDE_ENABLE)    // GA10x
#define LW_PTRIM_SYS_SWDIV_CLOCK_SEL_OVERRIDE_ENABLE    LW_PTRIM_SYS_XTAL4X_UNIV_SEC_CLK_CTRL_CLOCK_SEL_OVERRIDE_ENABLE
#endif
#endif

#ifndef LW_PTRIM_SYS_SWDIV_CLOCK_SEL_OVERRIDE_DISABLE
#if                                             defined(LW_PTRIM_SYS_DEFROST_UNIV_SEC_CLK_CTRL_CLOCK_SEL_OVERRIDE_DISABLE)  // Hopper and later
#define LW_PTRIM_SYS_SWDIV_CLOCK_SEL_OVERRIDE_DISABLE   LW_PTRIM_SYS_DEFROST_UNIV_SEC_CLK_CTRL_CLOCK_SEL_OVERRIDE_DISABLE
#elif                                           defined(LW_PTRIM_SYS_XTAL4X_UNIV_SEC_CLK_CTRL_CLOCK_SEL_OVERRIDE_DISABLE)   // GA10x
#define LW_PTRIM_SYS_SWDIV_CLOCK_SEL_OVERRIDE_DISABLE   LW_PTRIM_SYS_XTAL4X_UNIV_SEC_CLK_CTRL_CLOCK_SEL_OVERRIDE_DISABLE
#endif
#endif


/*******************************************************************************
    4-Input Switch Dividers
*******************************************************************************/

//
// SWDIV4 refers to 4-input switch dividers (automatic or otherwise), which are
// new as of GA100.  The defrost clock was called XTAL4X before Hopper, even
// though it was not 4x the xtal frequency.
//

// Make sure dev_trim has the same value in any chip family which defines both.
#ifdef LW_PTRIM_SYS_DEFROST_UNIV_SEC_CLK_CTRL_CLOCK_SOURCE_SEL
#ifdef LW_PTRIM_SYS_GLOBAL_SRC_CLK_SWITCH_DIVIDER_CLOCK_SOURCE_SEL
ct_assert(DRF_EXTENT(LW_PTRIM_SYS_DEFROST_UNIV_SEC_CLK_CTRL_CLOCK_SOURCE_SEL) == DRF_EXTENT(LW_PTRIM_SYS_GLOBAL_SRC_CLK_SWITCH_DIVIDER_CLOCK_SOURCE_SEL));
ct_assert(  DRF_BASE(LW_PTRIM_SYS_DEFROST_UNIV_SEC_CLK_CTRL_CLOCK_SOURCE_SEL) ==   DRF_BASE(LW_PTRIM_SYS_GLOBAL_SRC_CLK_SWITCH_DIVIDER_CLOCK_SOURCE_SEL));
#endif
#endif

#ifndef LW_PTRIM_SYS_SWDIV4_CLOCK_SOURCE_SEL
#if                                             defined(LW_PTRIM_SYS_DEFROST_UNIV_SEC_CLK_CTRL_CLOCK_SOURCE_SEL)        // Hopper after
#define LW_PTRIM_SYS_SWDIV4_CLOCK_SOURCE_SEL            LW_PTRIM_SYS_DEFROST_UNIV_SEC_CLK_CTRL_CLOCK_SOURCE_SEL
#elif                                           defined(LW_PTRIM_SYS_XTAL4X_UNIV_SEC_CLK_CTRL_CLOCK_SOURCE_SEL)         // GA10x
#define LW_PTRIM_SYS_SWDIV4_CLOCK_SOURCE_SEL            LW_PTRIM_SYS_XTAL4X_UNIV_SEC_CLK_CTRL_CLOCK_SOURCE_SEL
#elif                                           defined(LW_PTRIM_SYS_GLOBAL_SRC_CLK_SWITCH_DIVIDER_CLOCK_SOURCE_SEL)    // GA100/GA101
#define LW_PTRIM_SYS_SWDIV4_CLOCK_SOURCE_SEL            LW_PTRIM_SYS_GLOBAL_SRC_CLK_SWITCH_DIVIDER_CLOCK_SOURCE_SEL
#endif
#endif

//
// In the hardware manuals, these field values are named inconsistently among
// clock domains even within the same chip.  As such, it is best to hard-code
// the values here to introduce consistency in the software.  See bug 200400746.
// For 4-input SWDIVs, the register field is bitmapped.
//
#ifndef LW_PTRIM_SYS_SWDIV4_CLOCK_SOURCE_SEL_SOURCE
#define LW_PTRIM_SYS_SWDIV4_CLOCK_SOURCE_SEL_SOURCE(n)  BIT(n)
#endif


/*******************************************************************************
    2-Input Switch Dividers
*******************************************************************************/

//
// SWDIV2 refers to 2-input (automatic) switch dividers, which are new as of GA10x.
//
#ifndef LW_PTRIM_SYS_SWDIV2_CLOCK_SOURCE_SEL
#ifdef                                                  LW_PTRIM_SYS_PWRCLK_CTRL_CLOCK_SOURCE_SEL
#define LW_PTRIM_SYS_SWDIV2_CLOCK_SOURCE_SEL            LW_PTRIM_SYS_PWRCLK_CTRL_CLOCK_SOURCE_SEL
#endif
#endif

//
// In the hardware manuals, these field values are named inconsitently among
// clock domains even within the same chip.  As such, it is best to hard-code
// the values here to introduce consistency in the software.  See bug 200400746.
// For 2-input SWDIVs, the register field is one bit.
//
#ifndef LW_PTRIM_SYS_SWDIV2_CLOCK_SOURCE_SEL_SOURCE
#define LW_PTRIM_SYS_SWDIV2_CLOCK_SOURCE_SEL_SOURCE(n)  (n)
#endif


/*******************************************************************************
    PDIVs
*******************************************************************************/

//
// There are up to 4 dividers which share the logic in the ClkPDiv class.
// All chips which are supported as of November 2019 that have PDIVs (namely
// ga10x and g00x) have PDIVS on SPPLL1, but g00x does not have a PDIV on
// SPPLL0.
//
// It's important that, if they do exist, all the values must be equal since
// these values are referenced in the logic in 'clkpdiv.c'.  If that turns
// out not to be the case in some future chip, then the likely solution will
// be to add 'const LwU8' to 'ClkPDiv' which can be initialized in the
// schematic dag.
//
// As such, the values are defined based on SPPLL1, and undefined if there is
// another PDIV with a different value.
//

#ifndef LW_PVTRIM_SPPLL_PDIV_MIN
#ifdef                                      LW_PVTRIM_SYS_SPPLL1_COEFF2_PDIVA_MIN
#define LW_PVTRIM_SPPLL_PDIV_MIN            LW_PVTRIM_SYS_SPPLL1_COEFF2_PDIVA_MIN
#if (defined(LW_PVTRIM_SYS_SPPLL1_COEFF2_PDIVB_MIN) && LW_PVTRIM_SPPLL_PDIV_MIN != LW_PVTRIM_SYS_SPPLL1_COEFF2_PDIVB_MIN) || \
    (defined(LW_PVTRIM_SYS_SPPLL0_COEFF2_PDIVA_MIN) && LW_PVTRIM_SPPLL_PDIV_MIN != LW_PVTRIM_SYS_SPPLL0_COEFF2_PDIVA_MIN) || \
    (defined(LW_PVTRIM_SYS_SPPLL0_COEFF2_PDIVB_MIN) && LW_PVTRIM_SPPLL_PDIV_MIN != LW_PVTRIM_SYS_SPPLL0_COEFF2_PDIVB_MIN)
#undef LW_PVTRIM_SPPLL_PDIV_MIN
#endif
#endif
#endif

#ifndef LW_PVTRIM_SPPLL_PDIV_MAX
#ifdef                                      LW_PVTRIM_SYS_SPPLL1_COEFF2_PDIVA_MAX
#define LW_PVTRIM_SPPLL_PDIV_MAX            LW_PVTRIM_SYS_SPPLL1_COEFF2_PDIVA_MAX
#if (defined(LW_PVTRIM_SYS_SPPLL1_COEFF2_PDIVB_MAX) && LW_PVTRIM_SPPLL_PDIV_MAX != LW_PVTRIM_SYS_SPPLL1_COEFF2_PDIVB_MAX) || \
    (defined(LW_PVTRIM_SYS_SPPLL0_COEFF2_PDIVA_MAX) && LW_PVTRIM_SPPLL_PDIV_MAX != LW_PVTRIM_SYS_SPPLL0_COEFF2_PDIVA_MAX) || \
    (defined(LW_PVTRIM_SYS_SPPLL0_COEFF2_PDIVB_MAX) && LW_PVTRIM_SPPLL_PDIV_MAX != LW_PVTRIM_SYS_SPPLL0_COEFF2_PDIVB_MAX)
#undef LW_PVTRIM_SPPLL_PDIV_MAX
#endif
#endif
#endif

#ifndef LW_PVTRIM_SPPLL_PDIV_POWERDOWN
#ifdef                                      LW_PVTRIM_SYS_SPPLL1_COEFF2_PDIVA_POWERDOWN
#define LW_PVTRIM_SPPLL_PDIV_POWERDOWN      LW_PVTRIM_SYS_SPPLL1_COEFF2_PDIVA_POWERDOWN
#if (defined(LW_PVTRIM_SYS_SPPLL1_COEFF2_PDIVB_POWERDOWN) && LW_PVTRIM_SPPLL_PDIV_POWERDOWN != LW_PVTRIM_SYS_SPPLL1_COEFF2_PDIVB_POWERDOWN) || \
    (defined(LW_PVTRIM_SYS_SPPLL0_COEFF2_PDIVA_POWERDOWN) && LW_PVTRIM_SPPLL_PDIV_POWERDOWN != LW_PVTRIM_SYS_SPPLL0_COEFF2_PDIVA_POWERDOWN) || \
    (defined(LW_PVTRIM_SYS_SPPLL0_COEFF2_PDIVB_POWERDOWN) && LW_PVTRIM_SPPLL_PDIV_POWERDOWN != LW_PVTRIM_SYS_SPPLL0_COEFF2_PDIVB_POWERDOWN)
#undef LW_PVTRIM_SPPLL_PDIV_POWERDOWN
#endif
#endif
#endif


/*******************************************************************************
    LDIVs
*******************************************************************************/

//
// The LW_PTRIM_SYS_CLK_LDIV_DIV[01] fields are used in:
// - LDIVs, which have two dividers each (version 1),
// - OSMs, which have two dividers each (Version 1), and
// - SWDIVs, which have one divider each (Version 2),
//
// These use of these objects vary by family,  Specifically:
// - LDIVs exist in all chips (as of 2019),
// - OSMs exist in chips prior to Ampere, and
// - SWDIVs exist in Ampere and subsequent chips (bug 200400746).
//
// OSMs are obsolete in Ampere and after.  SWDIVs are the replacement.
//
// In dev_trim.h, _DIV0 and _DIV1 are named according to how the linear divider
// is attached in the schematic.  Since we need names that are generic with respect
// to the schematic, we use _DIV0, _DIV1, and _DIV(i).  Examples from Turing:
// _DIV0 => LW_PTRIM_SYS_GPC2CLK_REF_LDIV_ONESRC0DIV and LW_PTRIM_SYS_GPC2CLK_OUT_BYPDIV.
// _DIV1 => LW_PTRIM_SYS_GPC2CLK_REF_LDIV_ONESRC1DIV and LW_PTRIM_SYS_GPC2CLK_OUT_VCODIV.
//
// For Ampere and after, it's important that we use a definition for
// LW_PTRIM_SYS_CLK_LDIV_V2 which does not include the fractional divide
// indicator (bit 0).  For GA102 and after, we choose PWRCLK, which is defined
// as 5:1.  However, on GA100, all of the _DIVIDER_SEL fields are defined 5:0,
// 5:0, so we're forced to hard code the value here.  See comments in
// pmu_sw/inc/clk3/fs/clkldivv2.h for details.
//
// "_V2" means version 2.
//
#ifndef LW_PTRIM_SYS_CLK_LDIV_V2
#if                                     defined(LW_PTRIM_SYS_PWRCLK_CTRL_DIVIDER_SEL)   // GA102 and after
#define LW_PTRIM_SYS_CLK_LDIV_V2                LW_PTRIM_SYS_PWRCLK_CTRL_DIVIDER_SEL
#elif                                   defined(LW_PTRIM_SYS_GLOBAL_SRC_CLK_SWITCH_DIVIDER_DIVIDER_SEL)
#define LW_PTRIM_SYS_CLK_LDIV_V2                5:1                                     // GA100 and GA101
#endif
#endif

// See above
#ifndef LW_PTRIM_SYS_CLK_LDIV_DIV0
#if                                     defined(LW_PTRIM_GPC_GPC2CLK_OUT_BYPDIV)    // Turing and GA100/GA101
#define LW_PTRIM_SYS_CLK_LDIV_DIV0              LW_PTRIM_GPC_GPC2CLK_OUT_BYPDIV
#endif
#endif

// See above
#ifndef LW_PTRIM_SYS_CLK_LDIV_DIV1
#if                                     defined(LW_PTRIM_GPC_GPC2CLK_OUT_VCODIV)    // Turing and GA100/GA101
#define LW_PTRIM_SYS_CLK_LDIV_DIV1              LW_PTRIM_GPC_GPC2CLK_OUT_VCODIV
#endif
#endif

//
// Index a field that is not generally indexed in the manuals.
// __SIZE_1 is not defined because it is misleading and not used.
//
#ifdef  LW_PTRIM_SYS_CLK_LDIV_DIV0
#ifdef  LW_PTRIM_SYS_CLK_LDIV_DIV1
#define LW_PTRIM_SYS_CLK_LDIV_DIV(i)                                                                                                    \
  (DRF_EXTENT(LW_PTRIM_SYS_CLK_LDIV_DIV0) + (i) * (DRF_EXTENT(LW_PTRIM_SYS_CLK_LDIV_DIV1) - DRF_EXTENT(LW_PTRIM_SYS_CLK_LDIV_DIV0))) :  \
  (DRF_BASE  (LW_PTRIM_SYS_CLK_LDIV_DIV0) + (i) * (DRF_BASE  (LW_PTRIM_SYS_CLK_LDIV_DIV1) - DRF_BASE  (LW_PTRIM_SYS_CLK_LDIV_DIV0)))
#endif
#endif

#ifndef LW_PTRIM_SYS_CLK_LDIV_LOAD_CIP
#ifdef                                                  LW_PTRIM_SYS_GPC2CLK_REF_LDIV_LOAD_CIP
#define LW_PTRIM_SYS_CLK_LDIV_LOAD_CIP                  LW_PTRIM_SYS_GPC2CLK_REF_LDIV_LOAD_CIP
#endif
#endif

#ifndef LW_PTRIM_SYS_CLK_LDIV_GATECLKDLY
#ifdef                                                  LW_PTRIM_SYS_GPC2CLK_REF_LDIV_GATECLKDLY
#define LW_PTRIM_SYS_CLK_LDIV_GATECLKDLY                LW_PTRIM_SYS_GPC2CLK_REF_LDIV_GATECLKDLY
#endif
#endif

#ifndef LW_PTRIM_SYS_CLK_LDIV_GCLKS
#ifdef                                                  LW_PTRIM_SYS_GPC2CLK_REF_LDIV_GCLKS
#define LW_PTRIM_SYS_CLK_LDIV_GCLKS                     LW_PTRIM_SYS_GPC2CLK_REF_LDIV_GCLKS
#endif
#endif

#ifndef LW_PTRIM_SYS_CLK_LDIV_GCLKS_NO
#ifdef                                                  LW_PTRIM_SYS_GPC2CLK_REF_LDIV_GCLKS_NO
#define LW_PTRIM_SYS_CLK_LDIV_GCLKS_NO                  LW_PTRIM_SYS_GPC2CLK_REF_LDIV_GCLKS_NO
#endif
#endif

#ifndef LW_PTRIM_SYS_CLK_LDIV_GCLKS_YES
#ifdef                                                  LW_PTRIM_SYS_GPC2CLK_REF_LDIV_GCLKS_YES
#define LW_PTRIM_SYS_CLK_LDIV_GCLKS_YES                 LW_PTRIM_SYS_GPC2CLK_REF_LDIV_GCLKS_YES
#endif
#endif

#ifndef LW_PTRIM_SYS_CLK_LDIV_SDIV14
#ifdef                                                  LW_PTRIM_SYS_GPC2CLK_REF_LDIV_SDIV14
#define LW_PTRIM_SYS_CLK_LDIV_SDIV14                    LW_PTRIM_SYS_GPC2CLK_REF_LDIV_SDIV14
#endif
#endif

#ifndef LW_PTRIM_SYS_CLK_LDIV_SDIV14_INDIV1_MODE
#ifdef                                                  LW_PTRIM_SYS_GPC2CLK_REF_LDIV_SDIV14_INDIV1_MODE
#define LW_PTRIM_SYS_CLK_LDIV_SDIV14_INDIV1_MODE        LW_PTRIM_SYS_GPC2CLK_REF_LDIV_SDIV14_INDIV1_MODE
#endif
#endif

#ifndef LW_PTRIM_SYS_CLK_LDIV_SDIV14_INDIV4_MODE
#ifdef                                                  LW_PTRIM_SYS_GPC2CLK_REF_LDIV_SDIV14_INDIV4_MODE
#define LW_PTRIM_SYS_CLK_LDIV_SDIV14_INDIV4_MODE        LW_PTRIM_SYS_GPC2CLK_REF_LDIV_SDIV14_INDIV4_MODE
#endif
#endif


//
// Temporary defines per bug 200400746:
// OSMs are being replaced with SWDIVs in Ampere, but until all the HW changes
// are in, we need to support the OSMs.  These defines facilitate that.
// TODO: Remove these defines when Ampere HW changes are done.
//
#ifndef LW_PTRIM_SYS_CLK_SWITCH_STOPCLK
#define LW_PTRIM_SYS_CLK_SWITCH_STOPCLK                 28:28
#endif

#ifndef LW_PTRIM_SYS_CLK_SWITCH_STOPCLK_NO
#define LW_PTRIM_SYS_CLK_SWITCH_STOPCLK_NO              0
#endif

#ifndef LW_PTRIM_SYS_CLK_SWITCH_STOPCLK_YES
#define LW_PTRIM_SYS_CLK_SWITCH_STOPCLK_YES             1
#endif

#ifndef LW_PTRIM_SYS_CLK_SWITCH_FINALSEL
#define LW_PTRIM_SYS_CLK_SWITCH_FINALSEL                1:0
#endif

#ifndef LW_PTRIM_SYS_CLK_SWITCH_FINALSEL_SLOWCLK
#define LW_PTRIM_SYS_CLK_SWITCH_FINALSEL_SLOWCLK        0
#endif

#ifndef LW_PTRIM_SYS_CLK_SWITCH_FINALSEL_MISCCLK
#define LW_PTRIM_SYS_CLK_SWITCH_FINALSEL_MISCCLK        2
#endif

#ifndef LW_PTRIM_SYS_CLK_SWITCH_FINALSEL_ONESRCCLK
#define LW_PTRIM_SYS_CLK_SWITCH_FINALSEL_ONESRCCLK      3
#endif

#ifndef LW_PTRIM_SYS_CLK_SWITCH_MISCCLK
#define LW_PTRIM_SYS_CLK_SWITCH_MISCCLK                 26:24
#endif

#ifndef LW_PTRIM_SYS_CLK_SWITCH_SLOWCLK
#define LW_PTRIM_SYS_CLK_SWITCH_SLOWCLK                 17:16
#endif

#ifndef LW_PTRIM_SYS_CLK_SWITCH_ONESRCCLK
#define LW_PTRIM_SYS_CLK_SWITCH_ONESRCCLK               8:8
#endif

#ifndef LW_PTRIM_SYS_CLK_SWITCH_ONESRCCLK1_SELECT
#define LW_PTRIM_SYS_CLK_SWITCH_ONESRCCLK1_SELECT       30:30
#endif


/*******************************************************************************
    Noise Aware PLLs (NAPLLs)
*******************************************************************************/

#ifndef LW_PTRIM_SYS_PLL_CFG2_SDM_DIN
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG2_SDM_DIN
#define LW_PTRIM_SYS_PLL_CFG2_SDM_DIN                   LW_PTRIM_SYS_GPCPLL_CFG2_SDM_DIN
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG2_SDM_DIN)
#define LW_PTRIM_SYS_PLL_CFG2_SDM_DIN                   LW_PTRIM_GPC_BCAST_GPCPLL_CFG2_SDM_DIN
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG2_SDM_DIN_INIT
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG2_SDM_DIN_INIT
#define LW_PTRIM_SYS_PLL_CFG2_SDM_DIN_INIT              LW_PTRIM_SYS_GPCPLL_CFG2_SDM_DIN_INIT
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG2_SDM_DIN_INIT)
#define LW_PTRIM_SYS_PLL_CFG2_SDM_DIN_INIT              LW_PTRIM_GPC_BCAST_GPCPLL_CFG2_SDM_DIN_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG2_SDM_DIN_NEW
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG2_SDM_DIN_NEW
#define LW_PTRIM_SYS_PLL_CFG2_SDM_DIN_NEW               LW_PTRIM_SYS_GPCPLL_CFG2_SDM_DIN_NEW
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG2_SDM_DIN_NEW)
#define LW_PTRIM_SYS_PLL_CFG2_SDM_DIN_NEW               LW_PTRIM_GPC_BCAST_GPCPLL_CFG2_SDM_DIN_NEW
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG2_SDM_DIN_NEW_INIT
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG2_SDM_DIN_NEW_INIT
#define LW_PTRIM_SYS_PLL_CFG2_SDM_DIN_NEW_INIT          LW_PTRIM_SYS_GPCPLL_CFG2_SDM_DIN_NEW_INIT
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG2_SDM_DIN_NEW_INIT)
#define LW_PTRIM_SYS_PLL_CFG2_SDM_DIN_NEW_INIT          LW_PTRIM_GPC_BCAST_GPCPLL_CFG2_SDM_DIN_NEW_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG2_SETUP2
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG2_SETUP2
#define LW_PTRIM_SYS_PLL_CFG2_SETUP2                    LW_PTRIM_SYS_GPCPLL_CFG2_SETUP2
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG2_SETUP2)
#define LW_PTRIM_SYS_PLL_CFG2_SETUP2                    LW_PTRIM_GPC_BCAST_GPCPLL_CFG2_SETUP2
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG2_SETUP2_INIT
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG2_SETUP2_INIT
#define LW_PTRIM_SYS_PLL_CFG2_SETUP2_INIT               LW_PTRIM_SYS_GPCPLL_CFG2_SETUP2_INIT
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG2_SETUP2_INIT)
#define LW_PTRIM_SYS_PLL_CFG2_SETUP2_INIT               LW_PTRIM_GPC_BCAST_GPCPLL_CFG2_SETUP2_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG2_PLL_STEPA
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG2_PLL_STEPA
#define LW_PTRIM_SYS_PLL_CFG2_PLL_STEPA                 LW_PTRIM_SYS_GPCPLL_CFG2_PLL_STEPA
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG2_PLL_STEPA)
#define LW_PTRIM_SYS_PLL_CFG2_PLL_STEPA                 LW_PTRIM_GPC_BCAST_GPCPLL_CFG2_PLL_STEPA
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG2_PLL_STEPA_INIT
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG2_PLL_STEPA_INIT
#define LW_PTRIM_SYS_PLL_CFG2_PLL_STEPA_INIT            LW_PTRIM_SYS_GPCPLL_CFG2_PLL_STEPA_INIT
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG2_PLL_STEPA_INIT)
#define LW_PTRIM_SYS_PLL_CFG2_PLL_STEPA_INIT            LW_PTRIM_GPC_BCAST_GPCPLL_CFG2_PLL_STEPA_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS0_DFS_COEFF
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS0_DFS_COEFF
#define LW_PTRIM_SYS_PLL_DVFS0_DFS_COEFF                LW_PTRIM_SYS_GPCPLL_DVFS0_DFS_COEFF
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS0_DFS_COEFF_INIT
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS0_DFS_COEFF_INIT
#define LW_PTRIM_SYS_PLL_DVFS0_DFS_COEFF_INIT           LW_PTRIM_SYS_GPCPLL_DVFS0_DFS_COEFF_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS0_DFS_DET_MAX
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS0_DFS_DET_MAX
#define LW_PTRIM_SYS_PLL_DVFS0_DFS_DET_MAX              LW_PTRIM_SYS_GPCPLL_DVFS0_DFS_DET_MAX
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS0_DFS_DET_MAX_INIT
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS0_DFS_DET_MAX_INIT
#define LW_PTRIM_SYS_PLL_DVFS0_DFS_DET_MAX_INIT         LW_PTRIM_SYS_GPCPLL_DVFS0_DFS_DET_MAX_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS0_DFS_DC_OFFSET_INIT
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS0_DFS_DC_OFFSET_INIT
#define LW_PTRIM_SYS_PLL_DVFS0_DFS_DC_OFFSET_INIT       LW_PTRIM_SYS_GPCPLL_DVFS0_DFS_DC_OFFSET_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS0_SELVCO
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS0_SELVCO
#define LW_PTRIM_SYS_PLL_DVFS0_SELVCO                   LW_PTRIM_SYS_GPCPLL_DVFS0_SELVCO
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS0_SELVCO_INIT
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS0_SELVCO_INIT
#define LW_PTRIM_SYS_PLL_DVFS0_SELVCO_INIT              LW_PTRIM_SYS_GPCPLL_DVFS0_SELVCO_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS0_SELVDD0
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS0_SELVDD0
#define LW_PTRIM_SYS_PLL_DVFS0_SELVDD0                  LW_PTRIM_SYS_GPCPLL_DVFS0_SELVDD0
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS0_SELVDD0_INIT
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS0_SELVDD0_INIT
#define LW_PTRIM_SYS_PLL_DVFS0_SELVDD0_INIT             LW_PTRIM_SYS_GPCPLL_DVFS0_SELVDD0_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS0_SELVDD1
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS0_SELVDD1
#define LW_PTRIM_SYS_PLL_DVFS0_SELVDD1                  LW_PTRIM_SYS_GPCPLL_DVFS0_SELVDD1
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS0_SELVDD1_INIT
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS0_SELVDD1_INIT
#define LW_PTRIM_SYS_PLL_DVFS0_SELVDD1_INIT             LW_PTRIM_SYS_GPCPLL_DVFS0_SELVDD1_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS0_MODE
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS0_MODE
#define LW_PTRIM_SYS_PLL_DVFS0_MODE                     LW_PTRIM_SYS_GPCPLL_DVFS0_MODE
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS0_MODE_INIT
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS0_MODE_INIT
#define LW_PTRIM_SYS_PLL_DVFS0_MODE_INIT                LW_PTRIM_SYS_GPCPLL_DVFS0_MODE_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS0_MODE_DVFSPLL
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS0_MODE_DVFSPLL
#define LW_PTRIM_SYS_PLL_DVFS0_MODE_DVFSPLL             LW_PTRIM_SYS_GPCPLL_DVFS0_MODE_DVFSPLL
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS0_MODE_DVCO
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS0_MODE_DVCO
#define LW_PTRIM_SYS_PLL_DVFS0_MODE_DVCO                LW_PTRIM_SYS_GPCPLL_DVFS0_MODE_DVCO
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS0_EN_PLL_DYNRAMP
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS0_EN_PLL_DYNRAMP
#define LW_PTRIM_SYS_PLL_DVFS0_EN_PLL_DYNRAMP           LW_PTRIM_SYS_GPCPLL_DVFS0_EN_PLL_DYNRAMP
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS0_EN_PLL_DYNRAMP_INIT
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS0_EN_PLL_DYNRAMP_INIT
#define LW_PTRIM_SYS_PLL_DVFS0_EN_PLL_DYNRAMP_INIT      LW_PTRIM_SYS_GPCPLL_DVFS0_EN_PLL_DYNRAMP_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS0_EN_PLL_DYNRAMP_NO
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS0_EN_PLL_DYNRAMP_NO
#define LW_PTRIM_SYS_PLL_DVFS0_EN_PLL_DYNRAMP_NO        LW_PTRIM_SYS_GPCPLL_DVFS0_EN_PLL_DYNRAMP_NO
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS0_EN_PLL_DYNRAMP_YES
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS0_EN_PLL_DYNRAMP_YES
#define LW_PTRIM_SYS_PLL_DVFS0_EN_PLL_DYNRAMP_YES       LW_PTRIM_SYS_GPCPLL_DVFS0_EN_PLL_DYNRAMP_YES
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS0_DYNRAMP_DONE
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS0_DYNRAMP_DONE
#define LW_PTRIM_SYS_PLL_DVFS0_DYNRAMP_DONE             LW_PTRIM_SYS_GPCPLL_DVFS0_DYNRAMP_DONE
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS1_DFS_EXT_DET
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS1_DFS_EXT_DET
#define LW_PTRIM_SYS_PLL_DVFS1_DFS_EXT_DET              LW_PTRIM_SYS_GPCPLL_DVFS1_DFS_EXT_DET
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS1_DFS_EXT_DET_INIT
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS1_DFS_EXT_DET_INIT
#define LW_PTRIM_SYS_PLL_DVFS1_DFS_EXT_DET_INIT         LW_PTRIM_SYS_GPCPLL_DVFS1_DFS_EXT_DET_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS1_DFS_EXT_STRB
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS1_DFS_EXT_STRB
#define LW_PTRIM_SYS_PLL_DVFS1_DFS_EXT_STRB             LW_PTRIM_SYS_GPCPLL_DVFS1_DFS_EXT_STRB
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS1_DFS_EXT_STRB_INIT
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS1_DFS_EXT_STRB_INIT
#define LW_PTRIM_SYS_PLL_DVFS1_DFS_EXT_STRB_INIT        LW_PTRIM_SYS_GPCPLL_DVFS1_DFS_EXT_STRB_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS1_DFS_EXT_CAL
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS1_DFS_EXT_CAL
#define LW_PTRIM_SYS_PLL_DVFS1_DFS_EXT_CAL              LW_PTRIM_SYS_GPCPLL_DVFS1_DFS_EXT_CAL
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS1_DFS_EXT_CAL_INIT
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS1_DFS_EXT_CAL_INIT
#define LW_PTRIM_SYS_PLL_DVFS1_DFS_EXT_CAL_INIT         LW_PTRIM_SYS_GPCPLL_DVFS1_DFS_EXT_CAL_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS1_DFS_EXT_SEL
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS1_DFS_EXT_SEL
#define LW_PTRIM_SYS_PLL_DVFS1_DFS_EXT_SEL              LW_PTRIM_SYS_GPCPLL_DVFS1_DFS_EXT_SEL
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS1_DFS_EXT_SEL_INIT
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS1_DFS_EXT_SEL_INIT
#define LW_PTRIM_SYS_PLL_DVFS1_DFS_EXT_SEL_INIT         LW_PTRIM_SYS_GPCPLL_DVFS1_DFS_EXT_SEL_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS1_DFS_CTRL
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS1_DFS_CTRL
#define LW_PTRIM_SYS_PLL_DVFS1_DFS_CTRL                 LW_PTRIM_SYS_GPCPLL_DVFS1_DFS_CTRL
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS1_DFS_CTRL_INIT
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS1_DFS_CTRL_INIT
#define LW_PTRIM_SYS_PLL_DVFS1_DFS_CTRL_INIT            LW_PTRIM_SYS_GPCPLL_DVFS1_DFS_CTRL_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS1_DFS_CTRL
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS1_DFS_CTRL
#define LW_PTRIM_SYS_PLL_DVFS1_DFS_CTRL                 LW_PTRIM_SYS_GPCPLL_DVFS1_DFS_CTRL
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS1_DFS_CTRL_INIT
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS1_DFS_CTRL_INIT
#define LW_PTRIM_SYS_PLL_DVFS1_DFS_CTRL_INIT            LW_PTRIM_SYS_GPCPLL_DVFS1_DFS_CTRL_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS1_EN_SDM
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS1_EN_SDM
#define LW_PTRIM_SYS_PLL_DVFS1_EN_SDM                   LW_PTRIM_SYS_GPCPLL_DVFS1_EN_SDM
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS1_EN_SDM_INIT
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS1_EN_SDM_INIT
#define LW_PTRIM_SYS_PLL_DVFS1_EN_SDM_INIT              LW_PTRIM_SYS_GPCPLL_DVFS1_EN_SDM_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS1_EN_DFS
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS1_EN_DFS
#define LW_PTRIM_SYS_PLL_DVFS1_EN_DFS                   LW_PTRIM_SYS_GPCPLL_DVFS1_EN_DFS
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS1_EN_DFS_INIT
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS1_EN_DFS_INIT
#define LW_PTRIM_SYS_PLL_DVFS1_EN_DFS_INIT              LW_PTRIM_SYS_GPCPLL_DVFS1_EN_DFS_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS1_EN_DFS_CAL
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS1_EN_DFS_CAL
#define LW_PTRIM_SYS_PLL_DVFS1_EN_DFS_CAL               LW_PTRIM_SYS_GPCPLL_DVFS1_EN_DFS_CAL
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS1_EN_DFS_CAL_INIT
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS1_EN_DFS_CAL_INIT
#define LW_PTRIM_SYS_PLL_DVFS1_EN_DFS_CAL_INIT          LW_PTRIM_SYS_GPCPLL_DVFS1_EN_DFS_CAL_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS1_DFS_CAL_DONE
#ifdef                                                 LW_PTRIM_SYS_GPCPLL_DVFS1_DFS_CAL_DONE
#define LW_PTRIM_SYS_PLL_DVFS1_DFS_CAL_DONE            LW_PTRIM_SYS_GPCPLL_DVFS1_DFS_CAL_DONE
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG3_VCO_CTRL
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG3_VCO_CTRL
#define LW_PTRIM_SYS_PLL_CFG3_VCO_CTRL                  LW_PTRIM_SYS_GPCPLL_CFG3_VCO_CTRL
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG3_VCO_CTRL_INIT
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG3_VCO_CTRL_INIT
#define LW_PTRIM_SYS_PLL_CFG3_VCO_CTRL_INIT             LW_PTRIM_SYS_GPCPLL_CFG3_VCO_CTRL_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG3_PLL_STEPB
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG3_PLL_STEPB
#define LW_PTRIM_SYS_PLL_CFG3_PLL_STEPB                 LW_PTRIM_SYS_GPCPLL_CFG3_PLL_STEPB
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG3_PLL_STEPB)
#define LW_PTRIM_SYS_PLL_CFG3_PLL_STEPB                 LW_PTRIM_GPC_BCAST_GPCPLL_CFG3_PLL_STEPB
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG3_PLL_STEPB_INIT
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG3_PLL_STEPB_INIT
#define LW_PTRIM_SYS_PLL_CFG3_PLL_STEPB_INIT            LW_PTRIM_SYS_GPCPLL_CFG3_PLL_STEPB_INIT
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG3_PLL_STEPB_INIT)
#define LW_PTRIM_SYS_PLL_CFG3_PLL_STEPB_INIT            LW_PTRIM_GPC_BCAST_GPCPLL_CFG3_PLL_STEPB_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG3_DFS_TESTOUT
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_CFG3_DFS_TESTOUT
#define LW_PTRIM_SYS_PLL_CFG3_DFS_TESTOUT               LW_PTRIM_SYS_GPCPLL_CFG3_DFS_TESTOUT
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DVFS1_DFS_CTRL_9
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_DVFS1_DFS_CTRL_9
#define LW_PTRIM_SYS_PLL_DVFS1_DFS_CTRL_9               LW_PTRIM_SYS_GPCPLL_DVFS1_DFS_CTRL_9
#endif
#endif

// NOTE: These defines apply to analog PLLs, but not hybrid PLLs (HPLLs).
#ifndef LW_PVTRIM_SYS_PLL_CFG2_SSD_EN_SDM
#ifdef                                                  LW_PFB_FBPA_REFMPLL_CFG2_SSD_EN_SDM
#define LW_PVTRIM_SYS_PLL_CFG2_SSD_EN_SDM               LW_PFB_FBPA_REFMPLL_CFG2_SSD_EN_SDM
#endif
#endif

#ifndef LW_PVTRIM_SYS_PLL_CFG2_SSD_EN_SDM_INIT
#ifdef                                                  LW_PFB_FBPA_REFMPLL_CFG2_SSD_EN_SDM_INIT
#define LW_PVTRIM_SYS_PLL_CFG2_SSD_EN_SDM_INIT          LW_PFB_FBPA_REFMPLL_CFG2_SSD_EN_SDM_INIT
#endif
#endif

#ifndef LW_PVTRIM_SYS_PLL_CFG2_SSD_EN_SDM_YES
#ifdef                                                  LW_PFB_FBPA_REFMPLL_CFG2_SSD_EN_SDM_YES
#define LW_PVTRIM_SYS_PLL_CFG2_SSD_EN_SDM_YES           LW_PFB_FBPA_REFMPLL_CFG2_SSD_EN_SDM_YES
#endif
#endif

#ifndef LW_PVTRIM_SYS_PLL_CFG2_SSD_EN_SDM_NO
#ifdef                                                  LW_PFB_FBPA_REFMPLL_CFG2_SSD_EN_SDM_NO
#define LW_PVTRIM_SYS_PLL_CFG2_SSD_EN_SDM_NO            LW_PFB_FBPA_REFMPLL_CFG2_SSD_EN_SDM_NO
#endif
#endif

#ifndef LW_PVTRIM_SYS_PLL_CFG2_SSD_EN_SSC
#ifdef                                                  LW_PFB_FBPA_REFMPLL_CFG2_SSD_EN_SSC
#define LW_PVTRIM_SYS_PLL_CFG2_SSD_EN_SSC               LW_PFB_FBPA_REFMPLL_CFG2_SSD_EN_SSC
#endif
#endif

#ifndef LW_PVTRIM_SYS_PLL_CFG2_SSD_EN_SSC_INIT
#ifdef                                                  LW_PFB_FBPA_REFMPLL_CFG2_SSD_EN_SSC_INIT
#define LW_PVTRIM_SYS_PLL_CFG2_SSD_EN_SSC_INIT          LW_PFB_FBPA_REFMPLL_CFG2_SSD_EN_SSC_INIT
#endif
#endif

#ifndef LW_PVTRIM_SYS_PLL_CFG2_SSD_EN_SSC_YES
#ifdef                                                  LW_PFB_FBPA_REFMPLL_CFG2_SSD_EN_SSC_YES
#define LW_PVTRIM_SYS_PLL_CFG2_SSD_EN_SSC_YES           LW_PFB_FBPA_REFMPLL_CFG2_SSD_EN_SSC_YES
#endif
#endif

#ifndef LW_PVTRIM_SYS_PLL_CFG2_SSD_EN_SSC_NO
#ifdef                                                  LW_PFB_FBPA_REFMPLL_CFG2_SSD_EN_SSC_NO
#define LW_PVTRIM_SYS_PLL_CFG2_SSD_EN_SSC_NO            LW_PFB_FBPA_REFMPLL_CFG2_SSD_EN_SSC_NO
#endif
#endif

#ifndef LW_PVTRIM_SYS_PLL_CFG2_SSD_EN_DITHER
#ifdef                                                  LW_PFB_FBPA_REFMPLL_CFG2_SSD_EN_DITHER
#define LW_PVTRIM_SYS_PLL_CFG2_SSD_EN_DITHER            LW_PFB_FBPA_REFMPLL_CFG2_SSD_EN_DITHER
#endif
#endif

#ifndef LW_PVTRIM_SYS_PLL_CFG2_SSD_EN_DITHER_INIT
#ifdef                                                  LW_PFB_FBPA_REFMPLL_CFG2_SSD_EN_DITHER_INIT
#define LW_PVTRIM_SYS_PLL_CFG2_SSD_EN_DITHER_INIT       LW_PFB_FBPA_REFMPLL_CFG2_SSD_EN_DITHER_INIT
#endif
#endif

#ifndef LW_PVTRIM_SYS_PLL_CFG2_SSD_EN_DITHER_YES
#ifdef                                                  LW_PFB_FBPA_REFMPLL_CFG2_SSD_EN_DITHER_YES
#define LW_PVTRIM_SYS_PLL_CFG2_SSD_EN_DITHER_YES        LW_PFB_FBPA_REFMPLL_CFG2_SSD_EN_DITHER_YES
#endif
#endif

#ifndef LW_PVTRIM_SYS_PLL_CFG2_SSD_EN_DITHER_NO
#ifdef                                                  LW_PFB_FBPA_REFMPLL_CFG2_SSD_EN_DITHER_NO
#define LW_PVTRIM_SYS_PLL_CFG2_SSD_EN_DITHER_NO         LW_PFB_FBPA_REFMPLL_CFG2_SSD_EN_DITHER_NO
#endif
#endif

#ifndef LW_PVTRIM_SYS_PLL_CFG2_SSD_SDM_RESET
#ifdef                                                  LW_PFB_FBPA_REFMPLL_CFG2_SSD_SDM_RESET
#define LW_PVTRIM_SYS_PLL_CFG2_SSD_SDM_RESET            LW_PFB_FBPA_REFMPLL_CFG2_SSD_SDM_RESET
#endif
#endif

#ifndef LW_PVTRIM_SYS_PLL_CFG2_SSD_SDM_RESET_INIT
#ifdef                                                  LW_PFB_FBPA_REFMPLL_CFG2_SSD_SDM_RESET_INIT
#define LW_PVTRIM_SYS_PLL_CFG2_SSD_SDM_RESET_INIT       LW_PFB_FBPA_REFMPLL_CFG2_SSD_SDM_RESET_INIT
#endif
#endif

#ifndef LW_PVTRIM_SYS_PLL_CFG2_SSD_SDM_RESET_YES
#ifdef                                                  LW_PFB_FBPA_REFMPLL_CFG2_SSD_SDM_RESET_YES
#define LW_PVTRIM_SYS_PLL_CFG2_SSD_SDM_RESET_YES        LW_PFB_FBPA_REFMPLL_CFG2_SSD_SDM_RESET_YES
#endif
#endif

#ifndef LW_PVTRIM_SYS_PLL_CFG2_SSD_SDM_RESET_NO
#ifdef                                                  LW_PFB_FBPA_REFMPLL_CFG2_SSD_SDM_RESET_NO
#define LW_PVTRIM_SYS_PLL_CFG2_SSD_SDM_RESET_NO         LW_PFB_FBPA_REFMPLL_CFG2_SSD_SDM_RESET_NO
#endif
#endif

#ifndef LW_PVTRIM_SYS_PLL_SSD0_SDM_DIN
#ifdef                                                  LW_PFB_FBPA_REFMPLL_SSD0_SDM_DIN
#define LW_PVTRIM_SYS_PLL_SSD0_SDM_DIN                  LW_PFB_FBPA_REFMPLL_SSD0_SDM_DIN
#endif
#endif

#ifndef LW_PVTRIM_SYS_PLL_SSD0_SDM_DIN_INIT
#ifdef                                                  LW_PFB_FBPA_REFMPLL_SSD0_SDM_DIN_INIT
#define LW_PVTRIM_SYS_PLL_SSD0_SDM_DIN_INIT             LW_PFB_FBPA_REFMPLL_SSD0_SDM_DIN_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DYN_MODE
#ifdef                                                  LW_PTRIM_GPC_BCAST_GPCPLL_DYN_MODE
#define LW_PTRIM_SYS_PLL_DYN_MODE                       LW_PTRIM_GPC_BCAST_GPCPLL_DYN_MODE
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DYN_EN_PLL_DYNRAMP
#ifdef                                                  LW_PTRIM_GPC_BCAST_GPCPLL_DYN_EN_PLL_DYNRAMP
#define LW_PTRIM_SYS_PLL_DYN_EN_PLL_DYNRAMP             LW_PTRIM_GPC_BCAST_GPCPLL_DYN_EN_PLL_DYNRAMP
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DYN_EN_PLL_DYNRAMP_YES
#ifdef                                                  LW_PTRIM_GPC_BCAST_GPCPLL_DYN_EN_PLL_DYNRAMP_YES
#define LW_PTRIM_SYS_PLL_DYN_EN_PLL_DYNRAMP_YES         LW_PTRIM_GPC_BCAST_GPCPLL_DYN_EN_PLL_DYNRAMP_YES
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DYN_EN_SDM
#ifdef                                                  LW_PTRIM_GPC_BCAST_GPCPLL_DYN_EN_SDM
#define LW_PTRIM_SYS_PLL_DYN_EN_SDM                     LW_PTRIM_GPC_BCAST_GPCPLL_DYN_EN_SDM
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DYN_EN_SDM_YES
#ifdef                                                  LW_PTRIM_GPC_BCAST_GPCPLL_DYN_EN_SDM_YES
#define LW_PTRIM_SYS_PLL_DYN_EN_SDM_YES                 LW_PTRIM_GPC_BCAST_GPCPLL_DYN_EN_SDM_YES
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DYN_ENABLE_BG
#ifdef                                                  LW_PTRIM_GPC_BCAST_GPCPLL_DYN_ENABLE_BG
#define LW_PTRIM_SYS_PLL_DYN_ENABLE_BG                  LW_PTRIM_GPC_BCAST_GPCPLL_DYN_ENABLE_BG
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DYN_ENABLE_BG_INIT
#ifdef                                                  LW_PTRIM_GPC_BCAST_GPCPLL_DYN_ENABLE_BG_INIT
#define LW_PTRIM_SYS_PLL_DYN_ENABLE_BG_INIT             LW_PTRIM_GPC_BCAST_GPCPLL_DYN_ENABLE_BG_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DYN_EN_PLL_DYNRAMP_NO
#ifdef                                                  LW_PTRIM_GPC_BCAST_GPCPLL_DYN_EN_PLL_DYNRAMP_NO
#define LW_PTRIM_SYS_PLL_DYN_EN_PLL_DYNRAMP_NO          LW_PTRIM_GPC_BCAST_GPCPLL_DYN_EN_PLL_DYNRAMP_NO
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_DYN_DYNRAMP_DONE
#ifdef                                                  LW_PTRIM_GPC_BCAST_GPCPLL_DYN_DYNRAMP_DONE
#define LW_PTRIM_SYS_PLL_DYN_DYNRAMP_DONE               LW_PTRIM_GPC_BCAST_GPCPLL_DYN_DYNRAMP_DONE
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_CFG3_SDM_DIN_NEW
#ifdef                                                 LW_PTRIM_SYS_GPCPLL_CFG3_SDM_DIN_NEW
#define LW_PTRIM_SYS_PLL_CFG3_SDM_DIN_NEW              LW_PTRIM_SYS_GPCPLL_CFG3_SDM_DIN_NEW
#elif                                          defined(LW_PTRIM_GPC_BCAST_GPCPLL_CFG3_SDM_DIN_NEW)
#define LW_PTRIM_SYS_PLL_CFG3_SDM_DIN_NEW              LW_PTRIM_GPC_BCAST_GPCPLL_CFG3_SDM_DIN_NEW
#endif
#endif

#ifndef LW_PTRIM_SYS_PLL_COEFF_NDIV_NEW
#ifdef                                                 LW_PTRIM_SYS_SYSPLL_COEFF_NDIV_NEW
#define LW_PTRIM_SYS_PLL_COEFF_NDIV_NEW                LW_PTRIM_SYS_SYSPLL_COEFF_NDIV_NEW
#endif
#endif

#ifndef LW_PTRIM_CLK_OUT_BYPDIV
#ifdef                                                  LW_PTRIM_SYS_GPC2CLK_OUT_BYPDIV
#define LW_PTRIM_CLK_OUT_BYPDIV                         LW_PTRIM_SYS_GPC2CLK_OUT_BYPDIV
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPC2CLK_OUT_BYPDIV)
#define LW_PTRIM_CLK_OUT_BYPDIV                         LW_PTRIM_GPC_BCAST_GPC2CLK_OUT_BYPDIV
#endif
#endif

#ifndef LW_PTRIM_CLK_OUT_BYPDIV_BY1
#ifdef                                                  LW_PTRIM_SYS_GPC2CLK_OUT_BYPDIV_BY1
#define LW_PTRIM_CLK_OUT_BYPDIV_BY1                     LW_PTRIM_SYS_GPC2CLK_OUT_BYPDIV_BY1
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPC2CLK_OUT_BYPDIV_BY1)
#define LW_PTRIM_CLK_OUT_BYPDIV_BY1                     LW_PTRIM_GPC_BCAST_GPC2CLK_OUT_BYPDIV_BY1
#endif
#endif

#ifndef LW_PTRIM_CLK_OUT_BYPDIV_BY17
#ifdef                                                  LW_PTRIM_SYS_GPC2CLK_OUT_BYPDIV_BY17
#define LW_PTRIM_CLK_OUT_BYPDIV_BY17                    LW_PTRIM_SYS_GPC2CLK_OUT_BYPDIV_BY17
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPC2CLK_OUT_BYPDIV_BY17)
#define LW_PTRIM_CLK_OUT_BYPDIV_BY17                    LW_PTRIM_GPC_BCAST_GPC2CLK_OUT_BYPDIV_BY17
#endif
#endif

#ifndef LW_PTRIM_CLK_OUT_BYPDIV_BY31
#ifdef                                                  LW_PTRIM_SYS_GPC2CLK_OUT_BYPDIV_BY31
#define LW_PTRIM_CLK_OUT_BYPDIV_BY31                    LW_PTRIM_SYS_GPC2CLK_OUT_BYPDIV_BY31
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPC2CLK_OUT_BYPDIV_BY31)
#define LW_PTRIM_CLK_OUT_BYPDIV_BY31                    LW_PTRIM_GPC_BCAST_GPC2CLK_OUT_BYPDIV_BY31
#endif
#endif

#ifndef LW_PTRIM_CLK_OUT_VCODIV
#ifdef                                                  LW_PTRIM_SYS_GPC2CLK_OUT_VCODIV
#define LW_PTRIM_CLK_OUT_VCODIV                         LW_PTRIM_SYS_GPC2CLK_OUT_VCODIV
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPC2CLK_OUT_VCODIV)
#define LW_PTRIM_CLK_OUT_VCODIV                         LW_PTRIM_GPC_BCAST_GPC2CLK_OUT_VCODIV
#endif
#endif

#ifndef LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_NDIV_LO
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_NDIV_SLOWDOWN_NDIV_LO
#define LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_NDIV_LO           LW_PTRIM_SYS_GPCPLL_NDIV_SLOWDOWN_NDIV_LO
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_NDIV_SLOWDOWN_NDIV_LO)
#define LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_NDIV_LO           LW_PTRIM_GPC_BCAST_GPCPLL_NDIV_SLOWDOWN_NDIV_LO
#endif
#endif

#ifndef LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_NDIV_LO_MIN
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_NDIV_SLOWDOWN_NDIV_LO_MIN
#define LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_NDIV_LO_MIN       LW_PTRIM_SYS_GPCPLL_NDIV_SLOWDOWN_NDIV_LO_MIN
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_NDIV_SLOWDOWN_NDIV_LO_MIN)
#define LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_NDIV_LO_MIN       LW_PTRIM_GPC_BCAST_GPCPLL_NDIV_SLOWDOWN_NDIV_LO_MIN
#endif
#endif

#ifndef LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_NDIV_LO_MAX
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_NDIV_SLOWDOWN_NDIV_LO_MAX
#define LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_NDIV_LO_MAX       LW_PTRIM_SYS_GPCPLL_NDIV_SLOWDOWN_NDIV_LO_MAX
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_NDIV_SLOWDOWN_NDIV_LO_MAX)
#define LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_NDIV_LO_MAX                LW_PTRIM_GPC_BCAST_GPCPLL_NDIV_SLOWDOWN_NDIV_LO_MAX
#endif
#endif

#ifndef LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_NDIV_MID
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_NDIV_SLOWDOWN_NDIV_MID
#define LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_NDIV_MID          LW_PTRIM_SYS_GPCPLL_NDIV_SLOWDOWN_NDIV_MID
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_NDIV_SLOWDOWN_NDIV_MID)
#define LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_NDIV_MID          LW_PTRIM_GPC_BCAST_GPCPLL_NDIV_SLOWDOWN_NDIV_MID
#endif
#endif

#ifndef LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_NDIV_MID_MIN
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_NDIV_SLOWDOWN_NDIV_MID_MIN
#define LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_NDIV_MID_MIN      LW_PTRIM_SYS_GPCPLL_NDIV_SLOWDOWN_NDIV_MID_MIN
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_NDIV_SLOWDOWN_NDIV_MID_MIN)
#define LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_NDIV_MID_MIN      LW_PTRIM_GPC_BCAST_GPCPLL_NDIV_SLOWDOWN_NDIV_MID_MIN
#endif
#endif

#ifndef LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_NDIV_MID_MAX
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_NDIV_SLOWDOWN_NDIV_MID_MAX
#define LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_NDIV_MID_MAX      LW_PTRIM_SYS_GPCPLL_NDIV_SLOWDOWN_NDIV_MID_MAX
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_NDIV_SLOWDOWN_NDIV_MID_MAX)
#define LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_NDIV_MID_MAX      LW_PTRIM_GPC_BCAST_GPCPLL_NDIV_SLOWDOWN_NDIV_MID_MAX
#endif
#endif

#ifndef LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_EN_DYNRAMP
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_NDIV_SLOWDOWN_EN_DYNRAMP
#define LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_EN_DYNRAMP        LW_PTRIM_SYS_GPCPLL_NDIV_SLOWDOWN_EN_DYNRAMP
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_NDIV_SLOWDOWN_EN_DYNRAMP)
#define LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_EN_DYNRAMP        LW_PTRIM_GPC_BCAST_GPCPLL_NDIV_SLOWDOWN_EN_DYNRAMP
#endif
#endif

#ifndef LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_EN_DYNRAMP_YES
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_NDIV_SLOWDOWN_EN_DYNRAMP_YES
#define LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_EN_DYNRAMP_YES    LW_PTRIM_SYS_GPCPLL_NDIV_SLOWDOWN_EN_DYNRAMP_YES
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_NDIV_SLOWDOWN_EN_DYNRAMP_YES)
#define LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_EN_DYNRAMP_YES    LW_PTRIM_GPC_BCAST_GPCPLL_NDIV_SLOWDOWN_EN_DYNRAMP_YES
#endif
#endif

#ifndef LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_EN_DYNRAMP_NO
#ifdef                                                  LW_PTRIM_SYS_GPCPLL_NDIV_SLOWDOWN_EN_DYNRAMP_NO
#define LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_EN_DYNRAMP_NO     LW_PTRIM_SYS_GPCPLL_NDIV_SLOWDOWN_EN_DYNRAMP_NO
#elif                                           defined(LW_PTRIM_GPC_BCAST_GPCPLL_NDIV_SLOWDOWN_EN_DYNRAMP_NO)
#define LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_EN_DYNRAMP_NO     LW_PTRIM_GPC_BCAST_GPCPLL_NDIV_SLOWDOWN_EN_DYNRAMP_NO
#endif
#endif

#ifndef LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_SLOWDOWN_USING_PLL
#ifdef                                                          LW_PTRIM_SYS_GPCPLL_NDIV_SLOWDOWN_SLOWDOWN_USING_PLL
#define LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_SLOWDOWN_USING_PLL        LW_PTRIM_SYS_GPCPLL_NDIV_SLOWDOWN_SLOWDOWN_USING_PLL
#elif                                                   defined(LW_PTRIM_GPC_BCAST_GPCPLL_NDIV_SLOWDOWN_SLOWDOWN_USING_PLL)
#define LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_SLOWDOWN_USING_PLL        LW_PTRIM_GPC_BCAST_GPCPLL_NDIV_SLOWDOWN_SLOWDOWN_USING_PLL
#endif
#endif

#ifndef LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_SLOWDOWN_USING_PLL_YES
#ifdef                                                          LW_PTRIM_SYS_GPCPLL_NDIV_SLOWDOWN_SLOWDOWN_USING_PLL_YES
#define LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_SLOWDOWN_USING_PLL_YES    LW_PTRIM_SYS_GPCPLL_NDIV_SLOWDOWN_SLOWDOWN_USING_PLL_YES
#elif                                                   defined(LW_PTRIM_GPC_BCAST_GPCPLL_NDIV_SLOWDOWN_SLOWDOWN_USING_PLL_YES)
#define LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_SLOWDOWN_USING_PLL_YES    LW_PTRIM_GPC_BCAST_GPCPLL_NDIV_SLOWDOWN_SLOWDOWN_USING_PLL_YES
#endif
#endif

#ifndef LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_SLOWDOWN_USING_PLL_NO
#ifdef                                                          LW_PTRIM_SYS_GPCPLL_NDIV_SLOWDOWN_SLOWDOWN_USING_PLL_NO
#define LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_SLOWDOWN_USING_PLL_NO     LW_PTRIM_SYS_GPCPLL_NDIV_SLOWDOWN_SLOWDOWN_USING_PLL_NO
#elif                                                   defined(LW_PTRIM_GPC_BCAST_GPCPLL_NDIV_SLOWDOWN_SLOWDOWN_USING_PLL_NO)
#define LW_PTRIM_GPCPLL_NDIV_SLOWDOWN_SLOWDOWN_USING_PLL_NO     LW_PTRIM_GPC_BCAST_GPCPLL_NDIV_SLOWDOWN_SLOWDOWN_USING_PLL_NO
#endif
#endif


/*******************************************************************************
    Noise Aware FLLs (NAFLL)
*******************************************************************************/

#ifndef LW_PTRIM_NAFLL_COEFF_MDIV
#ifdef                                                  LW_PTRIM_GPC_GPCNAFLL_COEFF_MDIV
#define LW_PTRIM_NAFLL_COEFF_MDIV                       LW_PTRIM_GPC_GPCNAFLL_COEFF_MDIV
#endif
#endif

#ifndef LW_PTRIM_NAFLL_COEFF_PDIV
#ifdef                                                  LW_PTRIM_GPC_GPCNAFLL_COEFF_PDIV
#define LW_PTRIM_NAFLL_COEFF_PDIV                       LW_PTRIM_GPC_GPCNAFLL_COEFF_PDIV
#endif
#endif

#ifndef LW_PTRIM_NAFLL_COEFF_FLL_FRUG_MAIN
#ifdef                                                  LW_PTRIM_GPC_GPCNAFLL_COEFF_FLL_FRUG_MAIN
#define LW_PTRIM_NAFLL_COEFF_FLL_FRUG_MAIN              LW_PTRIM_GPC_GPCNAFLL_COEFF_FLL_FRUG_MAIN
#endif
#endif

#ifndef LW_PTRIM_NAFLL_COEFF_FLL_FRUG_FAST
#ifdef                                                  LW_PTRIM_GPC_GPCNAFLL_COEFF_FLL_FRUG_FAST
#define LW_PTRIM_NAFLL_COEFF_FLL_FRUG_FAST              LW_PTRIM_GPC_GPCNAFLL_COEFF_FLL_FRUG_FAST
#endif
#endif

#ifndef LW_PTRIM_LUT_SW_FREQ_REQ_LUT_VSELECT
#ifdef                                                  LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ_LUT_VSELECT
#define LW_PTRIM_LUT_SW_FREQ_REQ_LUT_VSELECT            LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ_LUT_VSELECT
#endif
#endif

#ifndef LW_PTRIM_LUT_SW_FREQ_REQ_LUT_VSELECT_INIT
#ifdef                                                  LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ_LUT_VSELECT_INIT
#define LW_PTRIM_LUT_SW_FREQ_REQ_LUT_VSELECT_INIT       LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ_LUT_VSELECT_INIT
#endif
#endif

#ifndef LW_PTRIM_LUT_SW_FREQ_REQ_LUT_VSELECT_MIN
#ifdef                                                  LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ_LUT_VSELECT_MIN
#define LW_PTRIM_LUT_SW_FREQ_REQ_LUT_VSELECT_MIN        LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ_LUT_VSELECT_MIN
#endif
#endif

#ifndef LW_PTRIM_LUT_SW_FREQ_REQ_LUT_VSELECT_LOGIC
#ifdef                                                  LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ_LUT_VSELECT_LOGIC
#define LW_PTRIM_LUT_SW_FREQ_REQ_LUT_VSELECT_LOGIC      LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ_LUT_VSELECT_LOGIC
#endif
#endif

#ifndef LW_PTRIM_LUT_SW_FREQ_REQ_LUT_VSELECT_SRAM
#ifdef                                                  LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ_LUT_VSELECT_SRAM
#define LW_PTRIM_LUT_SW_FREQ_REQ_LUT_VSELECT_SRAM       LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ_LUT_VSELECT_SRAM
#endif
#endif

#ifndef LW_PTRIM_LUT_SW_FREQ_REQ_SW_OVERRIDE_LUT
#ifdef                                                  LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ_SW_OVERRIDE_LUT
#define LW_PTRIM_LUT_SW_FREQ_REQ_SW_OVERRIDE_LUT        LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ_SW_OVERRIDE_LUT
#endif
#endif

#ifndef LW_PTRIM_LUT_SW_FREQ_REQ_SW_OVERRIDE_LUT_INIT
#ifdef                                                  LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ_SW_OVERRIDE_LUT_INIT
#define LW_PTRIM_LUT_SW_FREQ_REQ_SW_OVERRIDE_LUT_INIT   LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ_SW_OVERRIDE_LUT_INIT
#endif
#endif

#ifndef LW_PTRIM_LUT_SW_FREQ_REQ_SW_OVERRIDE_LUT_USE_HW_REQ
#ifdef                                                          LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ_SW_OVERRIDE_LUT_USE_HW_REQ
#define LW_PTRIM_LUT_SW_FREQ_REQ_SW_OVERRIDE_LUT_USE_HW_REQ     LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ_SW_OVERRIDE_LUT_USE_HW_REQ
#endif
#endif

#ifndef LW_PTRIM_LUT_SW_FREQ_REQ_SW_OVERRIDE_LUT_USE_SW_REQ
#ifdef                                                          LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ_SW_OVERRIDE_LUT_USE_SW_REQ
#define LW_PTRIM_LUT_SW_FREQ_REQ_SW_OVERRIDE_LUT_USE_SW_REQ     LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ_SW_OVERRIDE_LUT_USE_SW_REQ
#endif
#endif

#ifndef LW_PTRIM_LUT_SW_FREQ_REQ_SW_OVERRIDE_LUT_USE_MIN
#ifdef                                                          LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ_SW_OVERRIDE_LUT_USE_MIN
#define LW_PTRIM_LUT_SW_FREQ_REQ_SW_OVERRIDE_LUT_USE_MIN        LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ_SW_OVERRIDE_LUT_USE_MIN
#endif
#endif

#ifndef LW_PTRIM_LUT_SW_FREQ_REQ_NDIV
#ifdef                                                  LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ_NDIV
#define LW_PTRIM_LUT_SW_FREQ_REQ_NDIV                   LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ_NDIV
#endif
#endif

#ifndef LW_PTRIM_LUT_SW_FREQ_REQ_VFGAIN
#ifdef                                                  LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ_VFGAIN
#define LW_PTRIM_LUT_SW_FREQ_REQ_VFGAIN                 LW_PTRIM_GPC_GPCLUT_SW_FREQ_REQ_VFGAIN
#endif
#endif

#ifndef LW_PTRIM_LUT_DEBUG2_PRI_CTRL_STATE
#ifdef                                                  LW_PTRIM_GPC_GPCLUT_DEBUG2_PRI_CTRL_STATE
#define LW_PTRIM_LUT_DEBUG2_PRI_CTRL_STATE              LW_PTRIM_GPC_GPCLUT_DEBUG2_PRI_CTRL_STATE
#endif
#endif

#ifndef LW_PTRIM_LUT_DEBUG2_VFGAIN
#ifdef                                                  LW_PTRIM_GPC_GPCLUT_DEBUG2_VFGAIN
#define LW_PTRIM_LUT_DEBUG2_VFGAIN                      LW_PTRIM_GPC_GPCLUT_DEBUG2_VFGAIN
#endif
#endif

#ifndef LW_PTRIM_LUT_DEBUG2_NDIV
#ifdef                                                  LW_PTRIM_GPC_GPCLUT_DEBUG2_NDIV
#define LW_PTRIM_LUT_DEBUG2_NDIV                        LW_PTRIM_GPC_GPCLUT_DEBUG2_NDIV
#endif
#endif


/*******************************************************************************
    LWLink
*******************************************************************************/

#ifndef LW_PTRIM_SYS_LWLINK_UPHY_CTRL_UNIT2CLKS_PLL_TURN_OFF
#ifdef                                                                                         LW_PTRIM_SYS_LWLINK_UPHY_CTRL_UNIT2CLKS_PLL_TURN_OFF0
#define LW_PTRIM_SYS_LWLINK_UPHY_CTRL_UNIT2CLKS_PLL_TURN_OFF(i)                                (i):(i)
#define LW_PTRIM_SYS_LWLINK_UPHY_CTRL_UNIT2CLKS_PLL_TURN_OFF_INIT                              0x00000001 /* RWI-V */
#define LW_PTRIM_SYS_LWLINK_UPHY_CTRL_UNIT2CLKS_PLL_TURN_OFF_ASSERTED                          0x00000001 /* RW--V */
#define LW_PTRIM_SYS_LWLINK_UPHY_CTRL_UNIT2CLKS_PLL_TURN_OFF_DEASSERTED                        0x00000000 /* RW--V */
#endif
#endif

#ifndef LW_PTRIM_SYS_LWLINK_UPHY_CTRL_PHY2CLKS_USE_LOCKDET
#ifdef                                                                                         LW_PTRIM_SYS_LWLINK_UPHY_CTRL_PHY2CLKS_USE_LOCKDET0
#define LW_PTRIM_SYS_LWLINK_UPHY_CTRL_PHY2CLKS_USE_LOCKDET(i)                                  (i+4):(i+4)
#define LW_PTRIM_SYS_LWLINK_UPHY_CTRL_PHY2CLKS_USE_LOCKDET_INIT                                0x00000000 /* RWI-V */
#define LW_PTRIM_SYS_LWLINK_UPHY_CTRL_PHY2CLKS_USE_LOCKDET_ASSERTED                            0x00000001 /* RW--V */
#define LW_PTRIM_SYS_LWLINK_UPHY_CTRL_PHY2CLKS_USE_LOCKDET_DEASSERTED                          0x00000000 /* RW--V */
#endif
#endif



#ifndef LW_PTRIM_SYS_LWLINK_UPHY_CTRL_UNIT2CLKS_PLL_TURN_OFF__SIZE_1
#define LW_PTRIM_SYS_LWLINK_UPHY_CTRL_UNIT2CLKS_PLL_TURN_OFF__SIZE_1                           4
#endif

#ifndef LW_PTRIM_SYS_LWLINK_UPHY_OUTPUT_PLL_OFF
#ifdef                                                                                         LW_PTRIM_SYS_LWLINK_UPHY_OUTPUT_PLL_OFF0
#define LW_PTRIM_SYS_LWLINK_UPHY_OUTPUT_PLL_OFF(i)                                             (i+8):(i+8)
#define LW_PTRIM_SYS_LWLINK_UPHY_OUTPUT_PLL_OFF_ASSERTED                                       0x00000001 /* RW--V */
#define LW_PTRIM_SYS_LWLINK_UPHY_OUTPUT_PLL_OFF_DEASSERTED                                     0x00000000 /* RW--V */
#endif
#endif

#ifndef LW_PTRIM_SYS_LWLINK_UPHY_OUTPUT_PLL_OFF__SIZE_1
#define LW_PTRIM_SYS_LWLINK_UPHY_OUTPUT_PLL_OFF__SIZE_1                                        4
#endif

/*******************************************************************************
    VPLL related macros
*******************************************************************************/

#ifndef LW_PVTRIM_SYS_VPLL_SSD0_SDM_SSC_INCRMT
#ifdef                                                                  LW_PDISP_CLK_REM_VPLL_SSD0_SDM_SSC_INCRMT
#define LW_PVTRIM_SYS_VPLL_SSD0_SDM_SSC_INCRMT                          LW_PDISP_CLK_REM_VPLL_SSD0_SDM_SSC_INCRMT
#endif
#endif

#ifndef LW_PVTRIM_SYS_VPLL_SSD0_SDM_SSC_REPEAT
#ifdef                                                                  LW_PDISP_CLK_REM_VPLL_SSD0_SDM_SSC_REPEAT
#define LW_PVTRIM_SYS_VPLL_SSD0_SDM_SSC_REPEAT                          LW_PDISP_CLK_REM_VPLL_SSD0_SDM_SSC_REPEAT
#endif
#endif

#ifndef LW_PVTRIM_SYS_VPLL_SETUP_CONTROL_OVERRIDE
#ifdef                                                                  LW_PVTRIM_SYS_VPLL_MISC_SETUP_CONTROL_OVERRIDE
#define LW_PVTRIM_SYS_VPLL_SETUP_CONTROL_OVERRIDE                       LW_PVTRIM_SYS_VPLL_MISC_SETUP_CONTROL_OVERRIDE
#define LW_PVTRIM_SYS_VPLL_SETUP_CONTROL_OVERRIDE_INIT                  LW_PVTRIM_SYS_VPLL_MISC_SETUP_CONTROL_OVERRIDE_INIT
#define LW_PVTRIM_SYS_VPLL_SETUP_CONTROL_OVERRIDE_DISABLE               LW_PVTRIM_SYS_VPLL_MISC_SETUP_CONTROL_OVERRIDE_DISABLE
#define LW_PVTRIM_SYS_VPLL_SETUP_CONTROL_OVERRIDE_ENABLE                LW_PVTRIM_SYS_VPLL_MISC_SETUP_CONTROL_OVERRIDE_ENABLE
#elif                                                           defined(LW_PDISP_CLK_REM_VPLL_SETUP_CONTROL_OVERRIDE)
#define LW_PVTRIM_SYS_VPLL_SETUP_CONTROL_OVERRIDE                       LW_PDISP_CLK_REM_VPLL_SETUP_CONTROL_OVERRIDE
#define LW_PVTRIM_SYS_VPLL_SETUP_CONTROL_OVERRIDE_INIT                  LW_PDISP_CLK_REM_VPLL_SETUP_CONTROL_OVERRIDE_INIT
#define LW_PVTRIM_SYS_VPLL_SETUP_CONTROL_OVERRIDE_DISABLE               LW_PDISP_CLK_REM_VPLL_SETUP_CONTROL_OVERRIDE_DISABLE
#define LW_PVTRIM_SYS_VPLL_SETUP_CONTROL_OVERRIDE_ENABLE                LW_PDISP_CLK_REM_VPLL_SETUP_CONTROL_OVERRIDE_ENABLE
#endif
#endif

#ifndef LW_PVTRIM_SYS_VPLL_SETUP_CONTROL_STATUS_ENABLE
#ifdef                                                                  LW_PVTRIM_SYS_VPLL_MISC_SETUP_CONTROL_STATUS_ENABLE
#define LW_PVTRIM_SYS_VPLL_SETUP_CONTROL_STATUS_ENABLE                  LW_PVTRIM_SYS_VPLL_MISC_SETUP_CONTROL_STATUS_ENABLE
#define LW_PVTRIM_SYS_VPLL_SETUP_CONTROL_STATUS_ENABLE_INIT             LW_PVTRIM_SYS_VPLL_MISC_SETUP_CONTROL_STATUS_ENABLE_INIT
#define LW_PVTRIM_SYS_VPLL_SETUP_CONTROL_STATUS_ENABLE_NO               LW_PVTRIM_SYS_VPLL_MISC_SETUP_CONTROL_STATUS_ENABLE_NO
#define LW_PVTRIM_SYS_VPLL_SETUP_CONTROL_STATUS_ENABLE_YES              LW_PVTRIM_SYS_VPLL_MISC_SETUP_CONTROL_STATUS_ENABLE_YES
#elif                                                           defined(LW_PDISP_CLK_REM_VPLL_SETUP_CONTROL_STATUS_ENABLE)
#define LW_PVTRIM_SYS_VPLL_SETUP_CONTROL_STATUS_ENABLE                  LW_PDISP_CLK_REM_VPLL_SETUP_CONTROL_STATUS_ENABLE
#define LW_PVTRIM_SYS_VPLL_SETUP_CONTROL_STATUS_ENABLE_INIT             LW_PDISP_CLK_REM_VPLL_SETUP_CONTROL_STATUS_ENABLE_INIT
#define LW_PVTRIM_SYS_VPLL_SETUP_CONTROL_STATUS_ENABLE_NO               LW_PDISP_CLK_REM_VPLL_SETUP_CONTROL_STATUS_ENABLE_NO
#define LW_PVTRIM_SYS_VPLL_SETUP_CONTROL_STATUS_ENABLE_YES              LW_PDISP_CLK_REM_VPLL_SETUP_CONTROL_STATUS_ENABLE_YES
#endif
#endif

#ifndef LW_PVTRIM_SYS_VPLL_EXT_REF_CONFIG_EXT_REF
#ifdef                                                                  LW_PVTRIM_SYS_VPLL_MISC_EXT_REF_CONFIG_EXT_REF
#define LW_PVTRIM_SYS_VPLL_EXT_REF_CONFIG_EXT_REF                       LW_PVTRIM_SYS_VPLL_MISC_EXT_REF_CONFIG_EXT_REF
#define LW_PVTRIM_SYS_VPLL_EXT_REF_CONFIG_EXT_REF_INIT                  LW_PVTRIM_SYS_VPLL_MISC_EXT_REF_CONFIG_EXT_REF_INIT
#define LW_PVTRIM_SYS_VPLL_EXT_REF_CONFIG_EXT_REF_NOT_USED              LW_PVTRIM_SYS_VPLL_MISC_EXT_REF_CONFIG_EXT_REF_NOT_USED
#define LW_PVTRIM_SYS_VPLL_EXT_REF_CONFIG_EXT_REF_IN_USE                LW_PVTRIM_SYS_VPLL_MISC_EXT_REF_CONFIG_EXT_REF_IN_USE
#elif                                                           defined(LW_PDISP_CLK_REM_VPLL_EXT_REF_CONFIG_EXT_REF)
#define LW_PVTRIM_SYS_VPLL_EXT_REF_CONFIG_EXT_REF                       LW_PDISP_CLK_REM_VPLL_EXT_REF_CONFIG_EXT_REF
#define LW_PVTRIM_SYS_VPLL_EXT_REF_CONFIG_EXT_REF_INIT                  LW_PDISP_CLK_REM_VPLL_EXT_REF_CONFIG_EXT_REF_INIT
#define LW_PVTRIM_SYS_VPLL_EXT_REF_CONFIG_EXT_REF_NOT_USED              LW_PDISP_CLK_REM_VPLL_EXT_REF_CONFIG_EXT_REF_NOT_USED
#define LW_PVTRIM_SYS_VPLL_EXT_REF_CONFIG_EXT_REF_IN_USE                LW_PDISP_CLK_REM_VPLL_EXT_REF_CONFIG_EXT_REF_IN_USE
#endif
#endif

#ifndef LW_PVTRIM_SYS_VPLL_EXT_REF_CONFIG_SRC
#ifdef                                                                  LW_PVTRIM_SYS_VPLL_MISC_EXT_REF_CONFIG_SRC
#define LW_PVTRIM_SYS_VPLL_EXT_REF_CONFIG_SRC                           LW_PVTRIM_SYS_VPLL_MISC_EXT_REF_CONFIG_SRC
#define LW_PVTRIM_SYS_VPLL_EXT_REF_CONFIG_SRC_INIT                      LW_PVTRIM_SYS_VPLL_MISC_EXT_REF_CONFIG_SRC_INIT
#define LW_PVTRIM_SYS_VPLL_EXT_REF_CONFIG_SRC_EXT_REFCLKA_IB            LW_PVTRIM_SYS_VPLL_MISC_EXT_REF_CONFIG_SRC_EXT_REFCLKA_IB
#define LW_PVTRIM_SYS_VPLL_EXT_REF_CONFIG_SRC_EXT_REFCLKB_IB            LW_PVTRIM_SYS_VPLL_MISC_EXT_REF_CONFIG_SRC_EXT_REFCLKB_IB
#elif                                                           defined(LW_PDISP_CLK_REM_VPLL_EXT_REF_CONFIG_SRC)
#define LW_PVTRIM_SYS_VPLL_EXT_REF_CONFIG_SRC                           LW_PDISP_CLK_REM_VPLL_EXT_REF_CONFIG_SRC
#define LW_PVTRIM_SYS_VPLL_EXT_REF_CONFIG_SRC_INIT                      0x00000000
#define LW_PVTRIM_SYS_VPLL_EXT_REF_CONFIG_SRC_EXT_REFCLKA_IB            0x00000000
#define LW_PVTRIM_SYS_VPLL_EXT_REF_CONFIG_SRC_EXT_REFCLKB_IB            0x00000001
#endif
#endif

/*******************************************************************************
    Clock Monitor macros
*******************************************************************************/

#ifndef LW_PTRIM_SYS_FMON_THRESHOLD_HIGH_FMON_COUNT_THRESH_HIGH
#ifdef                                                                          LW_PTRIM_SYS_FMON_THRESHOLD_HIGH_SYSCLK_SYSCLK_FMON_COUNT_THRESH_HIGH
#define LW_PTRIM_SYS_FMON_THRESHOLD_HIGH_FMON_COUNT_THRESH_HIGH                 LW_PTRIM_SYS_FMON_THRESHOLD_HIGH_SYSCLK_SYSCLK_FMON_COUNT_THRESH_HIGH
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_THRESHOLD_HIGH_FMON_COUNT_THRESH_HIGH_INIT
#ifdef                                                                          LW_PTRIM_SYS_FMON_THRESHOLD_HIGH_SYSCLK_SYSCLK_FMON_COUNT_THRESH_HIGH_INIT
#define LW_PTRIM_SYS_FMON_THRESHOLD_HIGH_FMON_COUNT_THRESH_HIGH_INIT            LW_PTRIM_SYS_FMON_THRESHOLD_HIGH_SYSCLK_SYSCLK_FMON_COUNT_THRESH_HIGH_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_THRESHOLD_LOW_FMON_COUNT_THRESH_LOW
#ifdef                                                                          LW_PTRIM_SYS_FMON_THRESHOLD_LOW_SYSCLK_SYSCLK_FMON_COUNT_THRESH_LOW
#define LW_PTRIM_SYS_FMON_THRESHOLD_LOW_FMON_COUNT_THRESH_LOW                   LW_PTRIM_SYS_FMON_THRESHOLD_LOW_SYSCLK_SYSCLK_FMON_COUNT_THRESH_LOW
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_THRESHOLD_LOW_FMON_COUNT_THRESH_LOW_INIT
#ifdef                                                                          LW_PTRIM_SYS_FMON_THRESHOLD_LOW_SYSCLK_SYSCLK_FMON_COUNT_THRESH_LOW_INIT
#define LW_PTRIM_SYS_FMON_THRESHOLD_LOW_FMON_COUNT_THRESH_LOW_INIT              LW_PTRIM_SYS_FMON_THRESHOLD_LOW_SYSCLK_SYSCLK_FMON_COUNT_THRESH_LOW_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_REF_WINDOW_COUNT_FMON_REF_WINDOW_COUNT
#ifdef                                                                          LW_PTRIM_SYS_FMON_REF_WINDOW_COUNT_SYSCLK_SYSCLK_FMON_REF_WINDOW_COUNT
#define LW_PTRIM_SYS_FMON_REF_WINDOW_COUNT_FMON_REF_WINDOW_COUNT                LW_PTRIM_SYS_FMON_REF_WINDOW_COUNT_SYSCLK_SYSCLK_FMON_REF_WINDOW_COUNT
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_REF_WINDOW_COUNT_FMON_REF_WINDOW_COUNT_INIT
#ifdef                                                                          LW_PTRIM_SYS_FMON_REF_WINDOW_COUNT_SYSCLK_SYSCLK_FMON_REF_WINDOW_COUNT_INIT
#define LW_PTRIM_SYS_FMON_REF_WINDOW_COUNT_FMON_REF_WINDOW_COUNT_INIT           LW_PTRIM_SYS_FMON_REF_WINDOW_COUNT_SYSCLK_SYSCLK_FMON_REF_WINDOW_COUNT_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_REF_WINDOW_DC_CHECK_COUNT_FMON_REF_WINDOW_DC_CHECK_COUNT
#ifdef                                                                                  LW_PTRIM_SYS_FMON_REF_WINDOW_DC_CHECK_COUNT_SYSCLK_SYSCLK_FMON_REF_WINDOW_DC_CHECK_COUNT
#define LW_PTRIM_SYS_FMON_REF_WINDOW_DC_CHECK_COUNT_FMON_REF_WINDOW_DC_CHECK_COUNT      LW_PTRIM_SYS_FMON_REF_WINDOW_DC_CHECK_COUNT_SYSCLK_SYSCLK_FMON_REF_WINDOW_DC_CHECK_COUNT
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_REF_WINDOW_DC_CHECK_COUNT_FMON_REF_WINDOW_DC_CHECK_COUNT_INIT
#ifdef                                                                                  LW_PTRIM_SYS_FMON_REF_WINDOW_DC_CHECK_COUNT_SYSCLK_SYSCLK_FMON_REF_WINDOW_DC_CHECK_COUNT_INIT
#define LW_PTRIM_SYS_FMON_REF_WINDOW_DC_CHECK_COUNT_FMON_REF_WINDOW_DC_CHECK_COUNT_INIT LW_PTRIM_SYS_FMON_REF_WINDOW_DC_CHECK_COUNT_SYSCLK_SYSCLK_FMON_REF_WINDOW_DC_CHECK_COUNT_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_CONFIG_FMON_ENABLE
#ifdef                                                                          LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_ENABLE
#define LW_PTRIM_SYS_FMON_CONFIG_FMON_ENABLE                                    LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_ENABLE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_CONFIG_FMON_ENABLE_DISABLE
#ifdef                                                                          LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_ENABLE_DISABLE
#define LW_PTRIM_SYS_FMON_CONFIG_FMON_ENABLE_DISABLE                            LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_ENABLE_DISABLE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_CONFIG_FMON_ENABLE_ENABLE
#ifdef                                                                          LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_ENABLE_ENABLE
#define LW_PTRIM_SYS_FMON_CONFIG_FMON_ENABLE_ENABLE                             LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_ENABLE_ENABLE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_CONFIG_FMON_ENABLE_INIT
#ifdef                                                                          LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_ENABLE_INIT
#define LW_PTRIM_SYS_FMON_CONFIG_FMON_ENABLE_INIT                               LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_ENABLE_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_CONFIG_FMON_ENABLE_FMON_COUNTER
#ifdef                                                                          LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_ENABLE_FMON_COUNTER
#define LW_PTRIM_SYS_FMON_CONFIG_FMON_ENABLE_FMON_COUNTER                       LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_ENABLE_FMON_COUNTER
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_CONFIG_FMON_ENABLE_FMON_COUNTER_DISABLE
#ifdef                                                                          LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_ENABLE_FMON_COUNTER_DISABLE
#define LW_PTRIM_SYS_FMON_CONFIG_FMON_ENABLE_FMON_COUNTER_DISABLE               LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_ENABLE_FMON_COUNTER_DISABLE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_CONFIG_FMON_ENABLE_FMON_COUNTER_ENABLE
#ifdef                                                                          LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_ENABLE_FMON_COUNTER_ENABLE
#define LW_PTRIM_SYS_FMON_CONFIG_FMON_ENABLE_FMON_COUNTER_ENABLE                LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_ENABLE_FMON_COUNTER_ENABLE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_CONFIG_FMON_ENABLE_FMON_COUNTER_INIT
#ifdef                                                                          LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_ENABLE_FMON_COUNTER_INIT
#define LW_PTRIM_SYS_FMON_CONFIG_FMON_ENABLE_FMON_COUNTER_INIT                  LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_ENABLE_FMON_COUNTER_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_CONFIG_FMON_REF_CLK_WINDOW_EN
#ifdef                                                                          LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REF_CLK_WINDOW_EN
#define LW_PTRIM_SYS_FMON_CONFIG_FMON_REF_CLK_WINDOW_EN                         LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REF_CLK_WINDOW_EN
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_CONFIG_FMON_REF_CLK_WINDOW_EN_DISABLE
#ifdef                                                                          LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REF_CLK_WINDOW_EN_DISABLE
#define LW_PTRIM_SYS_FMON_CONFIG_FMON_REF_CLK_WINDOW_EN_DISABLE                 LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REF_CLK_WINDOW_EN_DISABLE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_CONFIG_FMON_REF_CLK_WINDOW_EN_ENABLE
#ifdef                                                                          LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REF_CLK_WINDOW_EN_ENABLE
#define LW_PTRIM_SYS_FMON_CONFIG_FMON_REF_CLK_WINDOW_EN_ENABLE                  LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REF_CLK_WINDOW_EN_ENABLE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_CONFIG_FMON_REF_CLK_WINDOW_EN_INIT
#ifdef                                                                          LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REF_CLK_WINDOW_EN_INIT
#define LW_PTRIM_SYS_FMON_CONFIG_FMON_REF_CLK_WINDOW_EN_INIT                    LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REF_CLK_WINDOW_EN_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_OVERFLOW_ERROR
#ifdef                                                                          LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_OVERFLOW_ERROR
#define LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_OVERFLOW_ERROR                     LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_OVERFLOW_ERROR
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_OVERFLOW_ERROR_DISABLE
#ifdef                                                                          LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_OVERFLOW_ERROR_DISABLE
#define LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_OVERFLOW_ERROR_DISABLE             LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_OVERFLOW_ERROR_DISABLE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_OVERFLOW_ERROR_ENABLE
#ifdef                                                                          LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_OVERFLOW_ERROR_ENABLE
#define LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_OVERFLOW_ERROR_ENABLE              LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_OVERFLOW_ERROR_ENABLE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_OVERFLOW_ERROR_INIT
#ifdef                                                                          LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_OVERFLOW_ERROR_INIT
#define LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_OVERFLOW_ERROR_INIT                LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_OVERFLOW_ERROR_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_LOW_THRESH_VIOL
#ifdef                                                                          LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_LOW_THRESH_VIOL
#define LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_LOW_THRESH_VIOL                    LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_LOW_THRESH_VIOL
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_LOW_THRESH_VIOL_DISABLE
#ifdef                                                                          LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_LOW_THRESH_VIOL_DISABLE
#define LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_LOW_THRESH_VIOL_DISABLE            LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_LOW_THRESH_VIOL_DISABLE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_LOW_THRESH_VIOL_ENABLE
#ifdef                                                                          LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_LOW_THRESH_VIOL_ENABLE
#define LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_LOW_THRESH_VIOL_ENABLE             LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_LOW_THRESH_VIOL_ENABLE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_LOW_THRESH_VIOL_INIT
#ifdef                                                                          LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_LOW_THRESH_VIOL_INIT
#define LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_LOW_THRESH_VIOL_INIT               LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_LOW_THRESH_VIOL_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_HIGH_THRESH_VIOL
#ifdef                                                                          LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_HIGH_THRESH_VIOL
#define LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_HIGH_THRESH_VIOL                   LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_HIGH_THRESH_VIOL
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_HIGH_THRESH_VIOL_DISABLE
#ifdef                                                                          LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_HIGH_THRESH_VIOL_DISABLE
#define LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_HIGH_THRESH_VIOL_DISABLE           LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_HIGH_THRESH_VIOL_DISABLE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_HIGH_THRESH_VIOL_ENABLE
#ifdef                                                                          LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_HIGH_THRESH_VIOL_ENABLE
#define LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_HIGH_THRESH_VIOL_ENABLE            LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_HIGH_THRESH_VIOL_ENABLE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_HIGH_THRESH_VIOL_INIT
#ifdef                                                                          LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_HIGH_THRESH_VIOL_INIT
#define LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_HIGH_THRESH_VIOL_INIT              LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_HIGH_THRESH_VIOL_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_DC_FAULT_VIOL
#ifdef                                                                          LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_DC_FAULT_VIOL
#define LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_DC_FAULT_VIOL                      LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_DC_FAULT_VIOL
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_DC_FAULT_VIOL_DISABLE
#ifdef                                                                          LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_DC_FAULT_VIOL_DISABLE
#define LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_DC_FAULT_VIOL_DISABLE              LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_DC_FAULT_VIOL_DISABLE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_DC_FAULT_VIOL_ENABLE
#ifdef                                                                          LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_DC_FAULT_VIOL_ENABLE
#define LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_DC_FAULT_VIOL_ENABLE               LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_DC_FAULT_VIOL_ENABLE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_DC_FAULT_VIOL_INIT
#ifdef                                                                          LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_DC_FAULT_VIOL_INIT
#define LW_PTRIM_SYS_FMON_CONFIG_FMON_REPORT_DC_FAULT_VIOL_INIT                 LW_PTRIM_SYS_FMON_CONFIG_SYSCLK_SYSCLK_FMON_REPORT_DC_FAULT_VIOL_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_REF_CLK_STATUS
#ifdef                                                                          LW_PTRIM_SYS_FMON_ENABLE_STATUS_SYSCLK_SYSCLK_FMON_ENABLE_FMON_REF_CLK_STATUS
#define LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_REF_CLK_STATUS         LW_PTRIM_SYS_FMON_ENABLE_STATUS_SYSCLK_SYSCLK_FMON_ENABLE_FMON_REF_CLK_STATUS
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_REF_CLK_STATUS_FALSE
#ifdef                                                                          LW_PTRIM_SYS_FMON_ENABLE_STATUS_SYSCLK_SYSCLK_FMON_ENABLE_FMON_REF_CLK_STATUS_FALSE
#define LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_REF_CLK_STATUS_FALSE   LW_PTRIM_SYS_FMON_ENABLE_STATUS_SYSCLK_SYSCLK_FMON_ENABLE_FMON_REF_CLK_STATUS_FALSE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_REF_CLK_STATUS_INIT
#ifdef                                                                          LW_PTRIM_SYS_FMON_ENABLE_STATUS_SYSCLK_SYSCLK_FMON_ENABLE_FMON_REF_CLK_STATUS_INIT
#define LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_REF_CLK_STATUS_INIT    LW_PTRIM_SYS_FMON_ENABLE_STATUS_SYSCLK_SYSCLK_FMON_ENABLE_FMON_REF_CLK_STATUS_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_REF_CLK_STATUS_TRUE
#ifdef                                                                          LW_PTRIM_SYS_FMON_ENABLE_STATUS_SYSCLK_SYSCLK_FMON_ENABLE_FMON_REF_CLK_STATUS_TRUE
#define LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_REF_CLK_STATUS_TRUE    LW_PTRIM_SYS_FMON_ENABLE_STATUS_SYSCLK_SYSCLK_FMON_ENABLE_FMON_REF_CLK_STATUS_TRUE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_ERR_CLK_STATUS
#ifdef                                                                          LW_PTRIM_SYS_FMON_ENABLE_STATUS_SYSCLK_SYSCLK_FMON_ENABLE_FMON_ERR_CLK_STATUS
#define LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_ERR_CLK_STATUS         LW_PTRIM_SYS_FMON_ENABLE_STATUS_SYSCLK_SYSCLK_FMON_ENABLE_FMON_ERR_CLK_STATUS
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_ERR_CLK_STATUS_FALSE
#ifdef                                                                          LW_PTRIM_SYS_FMON_ENABLE_STATUS_SYSCLK_SYSCLK_FMON_ENABLE_FMON_ERR_CLK_STATUS_FALSE
#define LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_ERR_CLK_STATUS_FALSE   LW_PTRIM_SYS_FMON_ENABLE_STATUS_SYSCLK_SYSCLK_FMON_ENABLE_FMON_ERR_CLK_STATUS_FALSE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_ERR_CLK_STATUS_INIT
#ifdef                                                                          LW_PTRIM_SYS_FMON_ENABLE_STATUS_SYSCLK_SYSCLK_FMON_ENABLE_FMON_ERR_CLK_STATUS_INIT
#define LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_ERR_CLK_STATUS_INIT    LW_PTRIM_SYS_FMON_ENABLE_STATUS_SYSCLK_SYSCLK_FMON_ENABLE_FMON_ERR_CLK_STATUS_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_ERR_CLK_STATUS_TRUE
#ifdef                                                                          LW_PTRIM_SYS_FMON_ENABLE_STATUS_SYSCLK_SYSCLK_FMON_ENABLE_FMON_ERR_CLK_STATUS_TRUE
#define LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_ERR_CLK_STATUS_TRUE    LW_PTRIM_SYS_FMON_ENABLE_STATUS_SYSCLK_SYSCLK_FMON_ENABLE_FMON_ERR_CLK_STATUS_TRUE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_CLK_STATUS
#ifdef                                                                          LW_PTRIM_SYS_FMON_ENABLE_STATUS_SYSCLK_SYSCLK_FMON_ENABLE_FMON_CLK_STATUS
#define LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_CLK_STATUS             LW_PTRIM_SYS_FMON_ENABLE_STATUS_SYSCLK_SYSCLK_FMON_ENABLE_FMON_CLK_STATUS
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_CLK_STATUS_FALSE
#ifdef                                                                          LW_PTRIM_SYS_FMON_ENABLE_STATUS_SYSCLK_SYSCLK_FMON_ENABLE_FMON_CLK_STATUS_FALSE
#define LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_CLK_STATUS_FALSE       LW_PTRIM_SYS_FMON_ENABLE_STATUS_SYSCLK_SYSCLK_FMON_ENABLE_FMON_CLK_STATUS_FALSE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_CLK_STATUS_INIT
#ifdef                                                                          LW_PTRIM_SYS_FMON_ENABLE_STATUS_SYSCLK_SYSCLK_FMON_ENABLE_FMON_CLK_STATUS_INIT
#define LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_CLK_STATUS_INIT        LW_PTRIM_SYS_FMON_ENABLE_STATUS_SYSCLK_SYSCLK_FMON_ENABLE_FMON_CLK_STATUS_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_CLK_STATUS_TRUE
#ifdef                                                                          LW_PTRIM_SYS_FMON_ENABLE_STATUS_SYSCLK_SYSCLK_FMON_ENABLE_FMON_CLK_STATUS_TRUE
#define LW_PTRIM_SYS_FMON_ENABLE_STATUS_FMON_ENABLE_FMON_CLK_STATUS_TRUE        LW_PTRIM_SYS_FMON_ENABLE_STATUS_SYSCLK_SYSCLK_FMON_ENABLE_FMON_CLK_STATUS_TRUE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_FAULT_CLEAR
#ifdef                                                                          LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_FAULT_CLEAR
#define LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_FAULT_CLEAR                         LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_FAULT_CLEAR
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_FAULT_CLEAR_FALSE
#ifdef                                                                          LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_FAULT_CLEAR_FALSE
#define LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_FAULT_CLEAR_FALSE                   LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_FAULT_CLEAR_FALSE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_FAULT_CLEAR_INIT
#ifdef                                                                          LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_FAULT_CLEAR_INIT
#define LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_FAULT_CLEAR_INIT                    LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_FAULT_CLEAR_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_FAULT_CLEAR_TRUE
#ifdef                                                                          LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_FAULT_CLEAR_TRUE
#define LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_FAULT_CLEAR_TRUE                    LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_FAULT_CLEAR_TRUE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_DC_FAULT
#ifdef                                                                          LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_DC_FAULT
#define LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_DC_FAULT                            LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_DC_FAULT
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_DC_FAULT_FALSE
#ifdef                                                                          LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_DC_FAULT_FALSE
#define LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_DC_FAULT_FALSE                      LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_DC_FAULT_FALSE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_DC_FAULT_INIT
#ifdef                                                                          LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_DC_FAULT_INIT
#define LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_DC_FAULT_INIT                       LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_DC_FAULT_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_DC_FAULT_TRUE
#ifdef                                                                          LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_DC_FAULT_TRUE
#define LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_DC_FAULT_TRUE                       LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_DC_FAULT_TRUE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_COUNT_LOWER_THRESH_HIGH_FAULT
#ifdef                                                                          LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_COUNT_LOWER_THRESH_HIGH_FAULT
#define LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_COUNT_LOWER_THRESH_HIGH_FAULT       LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_COUNT_LOWER_THRESH_HIGH_FAULT
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_COUNT_LOWER_THRESH_HIGH_FAULT_FALSE
#ifdef                                                                          LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_COUNT_LOWER_THRESH_HIGH_FAULT_FALSE
#define LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_COUNT_LOWER_THRESH_HIGH_FAULT_FALSE LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_COUNT_LOWER_THRESH_HIGH_FAULT_FALSE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_COUNT_LOWER_THRESH_HIGH_FAULT_INIT
#ifdef                                                                          LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_COUNT_LOWER_THRESH_HIGH_FAULT_INIT
#define LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_COUNT_LOWER_THRESH_HIGH_FAULT_INIT  LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_COUNT_LOWER_THRESH_HIGH_FAULT_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_COUNT_LOWER_THRESH_HIGH_FAULT_TRUE
#ifdef                                                                          LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_COUNT_LOWER_THRESH_HIGH_FAULT_TRUE
#define LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_COUNT_LOWER_THRESH_HIGH_FAULT_TRUE  LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_COUNT_LOWER_THRESH_HIGH_FAULT_TRUE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_COUNT_HIGHER_THRESH_HIGH_FAULT
#ifdef                                                                              LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_COUNT_HIGHER_THRESH_HIGH_FAULT
#define LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_COUNT_HIGHER_THRESH_HIGH_FAULT          LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_COUNT_HIGHER_THRESH_HIGH_FAULT
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_COUNT_HIGHER_THRESH_HIGH_FAULT_FALSE
#ifdef                                                                              LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_COUNT_HIGHER_THRESH_HIGH_FAULT_FALSE
#define LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_COUNT_HIGHER_THRESH_HIGH_FAULT_FALSE    LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_COUNT_HIGHER_THRESH_HIGH_FAULT_FALSE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_COUNT_HIGHER_THRESH_HIGH_FAULT_INIT
#ifdef                                                                              LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_COUNT_HIGHER_THRESH_HIGH_FAULT_INIT
#define LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_COUNT_HIGHER_THRESH_HIGH_FAULT_INIT     LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_COUNT_HIGHER_THRESH_HIGH_FAULT_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_COUNT_HIGHER_THRESH_HIGH_FAULT_TRUE
#ifdef                                                                              LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_COUNT_HIGHER_THRESH_HIGH_FAULT_TRUE
#define LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_COUNT_HIGHER_THRESH_HIGH_FAULT_TRUE     LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_COUNT_HIGHER_THRESH_HIGH_FAULT_TRUE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_OVERFLOW_ERROR
#ifdef                                                                          LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_OVERFLOW_ERROR
#define LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_OVERFLOW_ERROR                      LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_OVERFLOW_ERROR
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_OVERFLOW_ERROR_FALSE
#ifdef                                                                          LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_OVERFLOW_ERROR_FALSE
#define LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_OVERFLOW_ERROR_FALSE                LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_OVERFLOW_ERROR_FALSE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_OVERFLOW_ERROR_INIT
#ifdef                                                                          LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_OVERFLOW_ERROR_INIT
#define LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_OVERFLOW_ERROR_INIT                 LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_OVERFLOW_ERROR_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_OVERFLOW_ERROR_TRUE
#ifdef                                                                          LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_OVERFLOW_ERROR_TRUE
#define LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_OVERFLOW_ERROR_TRUE                 LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_OVERFLOW_ERROR_TRUE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_FAULT_OUT_STATUS
#ifdef                                                                          LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_FAULT_OUT_STATUS
#define LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_FAULT_OUT_STATUS                    LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_FAULT_OUT_STATUS
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_FAULT_OUT_STATUS_FALSE
#ifdef                                                                          LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_FAULT_OUT_STATUS_FALSE
#define LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_FAULT_OUT_STATUS_FALSE              LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_FAULT_OUT_STATUS_FALSE
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_FAULT_OUT_STATUS_INIT
#ifdef                                                                          LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_FAULT_OUT_STATUS_INIT
#define LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_FAULT_OUT_STATUS_INIT               LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_FAULT_OUT_STATUS_INIT
#endif
#endif

#ifndef LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_FAULT_OUT_STATUS_TRUE
#ifdef                                                                          LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_FAULT_OUT_STATUS_TRUE
#define LW_PTRIM_SYS_FMON_FAULT_STATUS_FMON_FAULT_OUT_STATUS_TRUE               LW_PTRIM_SYS_FMON_FAULT_STATUS_SYSCLK_SYSCLK_FMON_FAULT_OUT_STATUS_TRUE
#endif
#endif

