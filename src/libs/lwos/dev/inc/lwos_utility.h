/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LWOS_UTILITY_H
#define LWOS_UTILITY_H

/*!
 * @file    lwos_utility.h
 * @brief   LWOS non feature specific defines (common ones).
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ External Definitions ---------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * NOP macros.
 */
#define lwosNOP()           do { } while (LW_FALSE)
#define lwosVarNOP1(a)      do { (void)(a); } while (LW_FALSE)
#define lwosVarNOP2(a, ...) do { lwosVarNOP1(a); lwosVarNOP1(__VA_ARGS__); } while (LW_FALSE)
#define lwosVarNOP3(a, ...) do { lwosVarNOP1(a); lwosVarNOP2(__VA_ARGS__); } while (LW_FALSE)
#define lwosVarNOP4(a, ...) do { lwosVarNOP1(a); lwosVarNOP3(__VA_ARGS__); } while (LW_FALSE)
#define lwosVarNOP5(a, ...) do { lwosVarNOP1(a); lwosVarNOP4(__VA_ARGS__); } while (LW_FALSE)
#define lwosVarNOP6(a, ...) do { lwosVarNOP1(a); lwosVarNOP5(__VA_ARGS__); } while (LW_FALSE)
#define lwosVarNOP_GET_MACRO(c1, c2, c3, c4, c5, c6, NAME, ...) NAME
#define lwosVarNOP(...)                                                    \
    do                                                                     \
    {                                                                      \
        lwosVarNOP_GET_MACRO(__VA_ARGS__,                                  \
            lwosVarNOP6, lwosVarNOP5, lwosVarNOP4,                         \
            lwosVarNOP3, lwosVarNOP2, lwosVarNOP1, __unused)(__VA_ARGS__); \
    }                                                                      \
    while (LW_FALSE)

/*!
 * @brief   Common macros for basic Bit-Mask operations.
 */
#define LWOS_BM_ELEM(_pBM, _idx, _es)         _pBM[(_idx) / (_es)]
#define LWOS_BM_MASK(_idx, _es)              LWBIT((_idx) % (_es))
#define LWOS_BM_SET(_pBM, _idx, _es)                                           \
    do { LWOS_BM_ELEM(_pBM, _idx, _es) |=  LWOS_BM_MASK(_idx, _es); } while (LW_FALSE)
#define LWOS_BM_CLR(_pBM, _idx, _es)                                           \
    do { LWOS_BM_ELEM(_pBM, _idx, _es) &= ~LWOS_BM_MASK(_idx, _es); } while (LW_FALSE)
#define LWOS_BM_GET(_pBM, _idx, _es)                                           \
    (0 != (LWOS_BM_ELEM(_pBM, _idx, _es) & LWOS_BM_MASK(_idx, _es)))

/*!
 * Helper macro for mask initialization. Represents one unit (LwU8, LwU16, etc)
 * of the initial mask, with the entry index _entryIdx and size _es. Then, sets
 * the corresponding bit if _bit resides in this unit, or zero otherwise.
 *
 * Allows you to do something like this:
 *
 * #define ENTRIES(_eidx, _es) (LWOS_BM_INIT(0, (_eidx), (_es)) | \
 *                              LWOS_BM_INIT(15, (_eidx), (_es)))
 * And then:
 * LwU8 bitmask[2] = {ENTRIES(0, 8), ENTRIES(1, 8)};
 *
 * To set bits 0 and 15 in a 2-byte bitmask.
 */
#define LWOS_BM_INIT(_bit, _entryIdx, _es)  \
    (LWOS_BM_MASK((_bit), (_es)) & ((((_bit) / (_es)) == (_entryIdx)) ? (~0U) : (0U)))

/* ------------------------ Function Prototypes ----------------------------- */

#endif // LWOS_UTILITY_H
