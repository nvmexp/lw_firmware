/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SECTIONS_H
#define SECTIONS_H

/*
 * This file contains section definitions that should be used in code.
 * MK TODO: this file (probably) should not be part of kernel - it is up to
 * the application to select what is placed where.
 */

#define TO_STR(x) TO_STR2(x)
#define TO_STR2(x) #x
#define SECTION(x) __attribute__((section( x "." __FILE__ TO_STR(__LINE__))))

/*
 * Code marked as KERNEL is exelwtable only by kernel.
 * Tasks that are running at M privilege level should not expect being able
 * to access this code.
 *
 * Function starting with KERNEL belong to SafeRTOS. Ones starting with
 * sysKERNEL belong originated in lwpu.
 */

#define KERNEL_FUNCTION           SECTION( ".kernel_code" )
#define KERNEL_INIT_FUNCTION      SECTION( ".kernel_code_init" )
#define KERNEL_CREATE_FUNCTION    SECTION( ".kernel_code_init" )
#define KERNEL_DELETE_FUNCTION    SECTION( ".kernel_code_deinit" )
#define KERNEL_DATA               SECTION( ".kernel_data" )
#define KERNEL_DATA_MIRROR        SECTION( ".kernel_data" )

#define sysKERNEL_CODE        SECTION(".kernel_code")
#define sysKERNEL_CODE_INIT   SECTION(".kernel_code_init")
#define sysKERNEL_CODE_CREATE SECTION(".kernel_code_init")
#define sysKERNEL_DATA        SECTION(".kernel_data")
#define sysKERNEL_RODATA      SECTION(".kernel_rodata")

/*
 * Code marked as SHARED is shared between kernel and userspace and
 * resides in IMEM. This code should not have any global data.
 * Example functions are memset.
 */
#define sysSHARED_CODE        SECTION( ".shared_code" )
#define sysSHARED_CODE_INIT   SECTION( ".shared_code" )
#define sysSHARED_CODE_CREATE SECTION( ".shared_code" )
#define sysSHARED_DATA        SECTION( ".shared_data" )
#define sysSHARED_RODATA      SECTION( ".shared_rodata" )

/*
 * Code marked as SYSLIB is shared mostly between tasks. It resides in FB.
 */
#define sysSYSLIB_CODE      SECTION( ".syslib_code" )
#define sysSYSLIB_RODATA    SECTION( ".syslib_rodata" )

/*
 * All tasks code will be placed in that sections.
 * WARNING: Later tasks will be separated and this defines will go away.
 */
#define sysTASK_CODE        __attribute__( ( section( ".task_code" ) ) )
#define sysTASK_DATA        __attribute__( ( section( ".task_data" ) ) )
#define sysTASK_RODATA      __attribute__( ( section( ".task_rodata" ) ) )

#endif /* SECTIONS_H */
