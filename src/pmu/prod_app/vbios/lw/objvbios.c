/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

/*!
* @file   objvbios.c
* @brief  VBIOS common functions
*/

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "main_init_api.h"
#include "pmu_objvbios.h"
#include "vbios/vbios_dirt.h"
#include "vbios/vbios_dirt_parser.h"
#include "vbios/vbios_frts.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define PCI_EXP_ROM_SIGNATURE       0xaa55
#define PCI_EXP_ROM_SIGNATURE_LW    0x4e56 // "VN" in word format
#define IS_VALID_PCI_ROM_SIG(sig)   (((sig) == PCI_EXP_ROM_SIGNATURE) || \
                                     ((sig) == PCI_EXP_ROM_SIGNATURE_LW))

/* --------------------- Private functions Prototypes ----------------------- */
/* --------------------- External Declarations ------------------------------ */
/* --------------------- Global Variables ----------------------------------- */
OBJVBIOS Vbios
    GCC_ATTRIB_SECTION("dmem_vbios", "Vbios");

OBJVBIOS_PRE_INIT VbiosPreInit
    GCC_ATTRIB_SECTION("dmem_initData", "VbiosPreInit") =
{
#if PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS)
    .frts = {{0}},
#endif // PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS)
};

/* -------------------------- Public  functions ----------------------------- */
/*!
 * @brief   Constructs the basic software state for the global @ref Vbios object
 *
 * @return  @ref FLCN_OK    Success
 * @return  Others          Errors propagated from callees.
 */
FLCN_STATUS
constructVbios(void)
{
    FLCN_STATUS status = FLCN_OK;
    VBIOS_DIRT_PARSER dirtParser;

    // Verify that access to FRTS is valid.
    if (PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS))
    {
        VBIOS_FRTS *pFrts;

        PMU_ASSERT_OK_OR_GOTO(status,
            vbiosFrtsGet(&VbiosPreInit, &pFrts),
            constructVbios_exit);
        PMU_ASSERT_OK_OR_GOTO(status,
            vbiosFrtsConstructAndVerify(pFrts),
            constructVbios_exit);

        //
        // Populate the VBIOS_DIRT_PARSER with a pointer to FRTS so that FRTS
        // can be used for parsing
        //
        PMU_ASSERT_OK_OR_GOTO(status,
            vbiosDirtParserFrtsSet(&dirtParser, pFrts),
            constructVbios_exit);
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_VBIOS_DIRT))
    {
        VBIOS_DIRT *pDirt;

        PMU_ASSERT_OK_OR_GOTO(status,
            vbiosDirtGet(&Vbios, &pDirt),
            constructVbios_exit);
        PMU_ASSERT_OK_OR_GOTO(status,
            vbiosDirtConstruct(pDirt, &dirtParser),
            constructVbios_exit);
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_VBIOS_SKU))
    {
        VBIOS_SKU *pSku;

        PMU_ASSERT_OK_OR_GOTO(status,
            vbiosSkuGet(&Vbios, &pSku),
            constructVbios_exit);
        PMU_ASSERT_OK_OR_GOTO(status,
            vbiosSkuConstruct(pSku),
            constructVbios_exit);
    }

constructVbios_exit:
    return status;
}

/*!
* @brief  Check if VBIOS image is valid
*
* @param[in] pVbiosDesc  Flacon memory decriptor for the VBIOS image in FB
*
* @return FLCN_STATUS FLCN_OK if VBIOS is valid
*/
FLCN_STATUS
vbiosCheckValid
(
    RM_FLCN_MEM_DESC *pVbiosDesc
)
{
    LwU32 vbiosDWord0 = 0;
    FLCN_STATUS status;

    // Even if we need to read on WORD, min transfer size is one DWORD
    status = dmaRead(&vbiosDWord0, pVbiosDesc, 0, sizeof(vbiosDWord0));
    if (status != FLCN_OK)
    {
        return status;
    }

    if (!IS_VALID_PCI_ROM_SIG(vbiosDWord0 & 0xFFFF))
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    return FLCN_OK;
}
