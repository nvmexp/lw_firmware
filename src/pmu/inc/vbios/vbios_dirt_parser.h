/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @brief   Data structure representing an abstract way to construct the
 *          the table mappings in a @ref VBIOS_DIRT structure from various
 *          sources.
 */

#ifndef VBIOS_DIRT_PARSER_H
#define VBIOS_DIRT_PARSER_H

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "vbios/vbios_dirt.h"
#include "vbios/vbios_frts.h"

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Structure abstracting the different ways in which a @ref VBIOS_DIRT
 * structure can be populated.
 */
typedef struct VBIOS_DIRT_PARSER
{
#if PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS)
    /*!
     * Pointer to FRTS data to be used for parsing
     */
    VBIOS_FRTS *pFrts;
#else
    /*!
     * Only necessary for cases where this file is included from builds that
     * don't enable VBIOS_DIRT
     */
    LwU8 dummy;
#endif
} VBIOS_DIRT_PARSER;

/* ------------------------ Compile-Time Asserts ---------------------------- */
//
// If DIRT is enabled, we need at least one parser variant supported, so assert
// that here.
//
// Lwrrently, the only parser is CERT30, but additional variants should be added
// here in the future.
//
ct_assert(
    !PMUCFG_FEATURE_ENABLED(PMU_VBIOS_DIRT) ||
    PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS));

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief   Retrieves @ref VBIOS_DIRT_PARSER::pFrts, if enabled
 *
 * @param[in]   pParser Pointer to structure from which to retrieve
 * @param[out]  ppFrts  Pointer to pFrts field, if enabled; NULL otherwise
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  Any pointer argument was NULL
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
vbiosDirtParserFrtsGet
(
    VBIOS_DIRT_PARSER *pParser,
    VBIOS_FRTS **ppFrts
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pParser != NULL) &&
        (ppFrts != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosDirtParserFrtsGet_exit);

#if PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS)
    *ppFrts = pParser->pFrts;
#else // PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS)
    *ppFrts = NULL;
#endif // PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS)

vbiosDirtParserFrtsGet_exit:
    return status;
}

/*!
 * @brief   Sets @ref VBIOS_DIRT_PARSER::pFrts, if enabled
 *
 * @param[in]   pParser Pointer to structure in which to set
 * @param[out]  pFrts   Pointer to pFrts to set in structure
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  Any pointer argument was NULL
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
vbiosDirtParserFrtsSet
(
    VBIOS_DIRT_PARSER *pParser,
    VBIOS_FRTS *pFrts
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pParser != NULL) &&
        (pFrts != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosDirtParserFrtsSet_exit);

#if PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS)
    pParser->pFrts = pFrts;
#endif // PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS)

vbiosDirtParserFrtsSet_exit:
    return status;
}

/*!
 * @brief   Parses from a given source for @ref VBIOS_DIRT into the
 *          @ref VBIOS_DIRT structure
 *
 * @param[in]   pParser Point to structure to use for parsing
 * @param[out]  pDirt   Pointer to structure to populate
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  pParser is NULL
 * @return  @ref FLCN_ERR_ILWALID_STATE     Internal state is inconsistent.
 * @return  Others                          Errors propagated from callees.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
vbiosDirtParserParse
(
    VBIOS_DIRT_PARSER *pParser,
    VBIOS_DIRT *pDirt
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pParser != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosDirtParserParse_exit);

    if (PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS))
    {
        VBIOS_FRTS *pFrts;
        PMU_ASSERT_OK_OR_GOTO(status,
            vbiosDirtParserFrtsGet(pParser, &pFrts),
            vbiosDirtParserParse_exit);
        PMU_ASSERT_OK_OR_GOTO(status,
            vbiosFrtsDirtParse(pFrts, pDirt),
            vbiosDirtParserParse_exit);
    }
    else
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            FLCN_ERR_ILWALID_STATE,
            vbiosDirtParserParse_exit);
    }

vbiosDirtParserParse_exit:
    return status;
}

#endif // VBIOS_DIRT_PARSER_H
