/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_tempsim.c
 * @brief  Container-object for PMU Thermal routines. Contains generic non-HAL
 *         interrupt-routines plus logic required to hook-up chip-specific
 *         interrupt HAL-routines.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "pmu_selwrity.h"
#include "therm/objtherm.h"
#include "lib/lib_nocat.h"

#include "g_pmurpc.h"
#include "pmu_objfuse.h"
#include "dev_fuse.h"


/* ------------------------- Type Definitions ------------------------------- */
#if defined(FAULT_RECOVERY) && defined (LW_FUSE_OPT_PDI_0)
static void s_checkFaultHandlingTestTrigger(void);

#define CHECKFAULTHANDLINGTESTTRIGGER()    \
    s_checkFaultHandlingTestTrigger()
#else
#define CHECKFAULTHANDLINGTESTTRIGGER()
#endif // FAULT_RECOVERY
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
// Temperature used to trigger PMU breakpoint/NOCAT test path via tempsim
#define NOCAT_TEST_TARGET_TEMP RM_PMU_CELSIUS_TO_LW_TEMP(40)

/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Handles requests from RM to enable/disable TempSim.  Will arbitrate between
 * PMU and RM requests, such that RM requests always take priority, but last PMU
 * request (if any) will be restored when RM is disabled.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_THERM_TEMP_SIM
 */
FLCN_STATUS
pmuRpcThermTempSim
(
    RM_PMU_RPC_STRUCT_THERM_TEMP_SIM *pParams
)
{
    CHECKFAULTHANDLINGTESTTRIGGER();

    if (PMUCFG_FEATURE_ENABLED(PMU_NOCAT_TEMPSIM))
    {
        LwU32 data[] = { 0xA5A5A5A5, 0, 0x5A5A5A5A };
        data[1] = (LwU32)pParams->targetTemp;
#if !defined(INSTRUMENTATION_PROTECT)
        //
        // Critical NOCAT logging test
        // This path allows PMU to be halted by external input, so it's guarded
        // by INSTRUMENTATION_PROTECT feature flag that is only enabled in
        // "Instrument Ucodes" build on DVS to keep this out of external builds
        //
        if (pParams->targetTemp == NOCAT_TEST_TARGET_TEMP)
        {
            data[0] = 0xABCD1234;
            data[2] = 0x1234ABCD;
            (void)nocatDiagBufferSet(data, (sizeof(data) / sizeof(LwU32)));
            PMU_BREAKPOINT();
        }
#endif // !INSTRUMENTATION_PROTECT

        // This is an example (to be removed once actual use-cases are added).
        (void)nocatDiagBufferLog(data, LW_ARRAY_ELEMENTS(data), LW_FALSE);
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PRIV_SEC) &&
        (FLD_TEST_DRF(_VSB, _REG0, _TEMP_SIM, _DISALLOWED, PmuVSBCache)))
    {
        return FLCN_ERR_PRIV_SEC_VIOLATION;
    }

    return thermTempSimSetControl_HAL(&Therm, pParams->bSimulate,
                                              pParams->targetTemp);
}

#if defined(FAULT_RECOVERY) && defined (LW_FUSE_OPT_PDI_0)

/*
** constants used for PDIs.
** Security requires the PDIs be specified in code (vs listed in a table)
** to insure they are not tampered with
** The switch statment below should satisfy that requirement
*/
#define ROSBORNE_RTX_2080_TEST_PDI (0x004c3ee5c98883daULL)
#define QA_GRID_TESLA_T4_TEST_PDI  (0x2DC93F5ADD1549EBULL)
#define QA_QUADRO_RTX8000_TEST_PDI (0xE66108882BEC3A52ULL)
#define QA_QUADRO_RTX4000_TEST_PDI (0x48DFC8364B4E006DULL)
#define QA_GEFORCE_TU104_TEST_PDI  (0x7190C64FCB7B90BFULL)
#define QA_GEFORCE_TU102_TEST_PDI  (0xD3B4832017719F01ULL)
#define QA_GEFORCE_TU106_TEST_PDI  (0xA0878CD1AA658F65ULL)
#define QA_GEFORCE_TU116_TEST_PDI  (0x4AF1F22A270FEC8DULL)
#define QA_GEFORCE_TU117_TEST_PDI  (0x4C2887FFB7D4B7ADULL)

static void
s_checkFaultHandlingTestTrigger()
{
    FLCN_U64    pdi;

    // BO-TODO: This is SUPER temporary code. This will be replaced with a purpose made
    //          RPC mechanism that will do the pdi check and breakpoint conditionally
    //          based on its parameters.
    pdi.data = 0;
    pdi.parts.lo = fuseRead(LW_FUSE_OPT_PDI_0);
    pdi.parts.hi = fuseRead(LW_FUSE_OPT_PDI_1);

    switch (pdi.data)
    {
    case ROSBORNE_RTX_2080_TEST_PDI:
    case QA_GRID_TESLA_T4_TEST_PDI:
    case QA_QUADRO_RTX8000_TEST_PDI:
    case QA_QUADRO_RTX4000_TEST_PDI:
    case QA_GEFORCE_TU104_TEST_PDI:
    case QA_GEFORCE_TU102_TEST_PDI:
    case QA_GEFORCE_TU106_TEST_PDI:
    case QA_GEFORCE_TU116_TEST_PDI:
    case QA_GEFORCE_TU117_TEST_PDI:
        {
            PMU_BREAKPOINT();
            break;
        }
    }
}

#endif // FAULT_RECOVERY

/* ------------------------- Private Functions ------------------------------ */

