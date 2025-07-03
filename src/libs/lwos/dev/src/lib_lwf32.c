/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    lib_lwf32.c
 * @brief   Library of LwF32 (32-bit float) operations
 *
 * GCC compiler has to emulate floating point math when generating falcon code,
 * therefore most common operations were extracted to a library fucntions.
 *
 * @note    LwF32 <=> float (IEEE-754 binary32)
 *
 * @note    To allow quick implementation changes and to aid code profiling all
 *          operations were wrapped by macros or helper fucntions.
 *          Do NOT call native GCC code directly!!!
 *
 * @warning lwF32Mul() & lwF32Div() depend on 64-bit multiplication (.libLw64).
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "lib_lwf32.h"

/* ------------------------- Type Definitions ------------------------------- */
union Generic32BitNum {
    LwF32 *f;
    LwU32 *u;
};

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Function Prototypes --------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Function wrapper performing LwU32 -> LwF32 colwersion.
 *
 * @param[in]      value        The value to colwert
 * 
 * @return The colwerted value
 * 
 * @note    Introduced to allow quick implementation changes and aid profiling.
 *          Do NOT call native GCC code directly!!!
 * @note    Colwersion takes approximately same time as ADD / SUB.
 */
LwF32
lwF32ColwertFromU32
(
    LwU32 value
)
{
    return (LwF32) value;
}

/*!
 * @brief   Function wrapper performing LwS32 -> LwF32 colwersion.
 *
 * @param[in]      value        The value to colwert
 * 
 * @return The colwerted value
 * 
 * @note    Introduced to allow quick implementation changes and aid profiling.
 *          Do NOT call native GCC code directly!!!
 * @note    Colwersion takes approximately same time as ADD / SUB.
 */
LwF32
lwF32ColwertFromS32
(
    LwS32 value
)
{
    return (LwF32) value;
}

/*!
 * @brief   Function wrapper performing LwF32 -> LwU32 colwersion.
 *
 * @param[in]      value        The value to colwert
 * 
 * @return The colwerted value
 * 
 * @note    Introduced to allow quick implementation changes and aid profiling.
 *          Do NOT call native GCC code directly!!!
 * @note    Colwersion takes approximately same time as ADD / SUB.
 */
LwU32
lwF32ColwertToU32
(
    LwF32 value
)
{
    return (LwU32) value;
}

/*!
 * @brief   Function wrapper performing LwF32 -> LwS32 colwersion.
 *
 * @param[in]      value        The value to colwert
 * 
 * @return The colwerted value
 * 
 * @note    Introduced to allow quick implementation changes and aid profiling.
 *          Do NOT call native GCC code directly!!!
 * @note    Colwersion takes approximately same time as ADD / SUB.
 */
LwS32
lwF32ColwertToS32
(
    LwF32 value
)
{
    return (LwS32) value;
}

/*!
 * @brief   Function wrapper performing LwU32 -> LwF32 mapping.
 *
 * While the colwersion routine @ref lwF32ColwertFromU32 performs IEEE-754 encoding,
 * this mapping routine preserves the binary format of the input value and only
 * changes its type.
 *
 * @param[in]      pValue        The value to map
 * 
 * @return The mapped value
 * 
 * @note    Introduced to allow quick implementation changes and aid profiling.
 *          Do NOT call native GCC code directly!!!
 */
LwF32
lwF32MapFromU32
(
    LwU32 *pValue
)
{
    union Generic32BitNum value;
    value.u = pValue;
    return *(value.f);
}

/*!
 * @brief   Function wrapper performing LwF32 -> LwU32 mapping.
 *
 * While the colwersion routine @ref lwF32ColwertToU32 performs IEEE-754 decoding,
 * this mapping routine preserves the binary format of the input value and only
 * changes its type.
 *
 * @param[in]      pValue        The value to map
 * 
 * @return The mapped value
 * 
 * @note    Introduced to allow quick implementation changes and aid profiling.
 *          Do NOT call native GCC code directly!!!
 */
LwU32
lwF32MapToU32
(
    LwF32 *pValue
)
{
    union Generic32BitNum value;
    value.f = pValue;
    return *(value.u);
}

/*!
 * @brief   Function wrapper performing LwU64 -> LwF32 colwersion.
 *
 * @param[in]      value        The value to colwert
 *
 * @return The colwerted value
 *
 * @note    Introduced to allow quick implementation changes and aid profiling.
 *          Do NOT call native GCC code directly!!!
 * @note    Colwersion takes approximately same time as ADD / SUB.
 */
LwF32
lwF32ColwertFromU64
(
    LwU64 value
)
{
    return (LwF32) value;
}

/*!
 * @brief   Function wrapper performing LwF32 -> LwU64 colwersion.
 *
 * @param[in]      value        The value to colwert
 *
 * @return The colwerted value
 *
 * @note    Introduced to allow quick implementation changes and aid profiling.
 *          Do NOT call native GCC code directly!!!
 * @note    Colwersion takes approximately same time as ADD / SUB.
 */
LwU64
lwF32ColwertToU64
(
    LwF32 value
)
{
    return (LwU64) value;
}

/* ------------------------- Private Functions ------------------------------ */
