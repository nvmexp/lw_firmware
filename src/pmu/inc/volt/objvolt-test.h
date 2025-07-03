/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef OBJVOLT_TEST_H
#define OBJVOLT_TEST_H

/*!
 * @file objvolt-test.h
 *
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Application Includes --------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/*!
 * @brief      Macro to generate the pre-test function prototype.
 *
 * @param[in]  name  The name of the test.
 *
 * @return     The pre-test function prototype.
 */
#define PRE_TEST_METHOD(name) \
    static void PRE_TEST_NAME(name)()

/*!
 * @brief      Macro to generate the pre-test function name.
 *
 * @param[in]  name  The name of the test.
 *
 * @return     The pre-test function name.
 */
#define PRE_TEST_NAME(name) \
    s_##name##PreTest

/*!
 * @brief      Macro to generate the post-test function prototype.
 *
 * @param[in]  name  The name of the test.
 *
 * @return     The post-test function prototype.
 */
#define POST_TEST_METHOD(name) \
    static void POST_TEST_NAME(name)()

/*!
 * @brief      Macro to generate the post-test function name.
 *
 * @param[in]  name  The name of the test.
 *
 * @return     The post-test function name.
 */
#define POST_TEST_NAME(name) \
    s_##name##PostTest
/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Include Derived Types -------------------------- */

#endif // OBJVOLT_TEST_H
