/*
 * Copyright (c) 2019-2021, LWPU Corporation. All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 * Header file for LWPKA engine.
 */

#ifndef INCLUDED_TEGRA_LWPKA_H
#define INCLUDED_TEGRA_LWPKA_H

#ifdef DOXYGEN
#include <ccc_doxygen.h>
#endif

#include <ccc_lwrm_drf.h>

/* include the HW header for reg offsets.
 * Always use the matching version from VDK distribution!
 *
 * FIXME: This will need to be reworked anyway => do like this for now.
 */
#include <dev_se_lwpka.h>

/*******************************/

/* Large Operand Registers defined in lwpka. Not all of these regs are used
 * by all operations; see LWPKA IAS register use per operation.
 *
 * LOR_S : Source register
 * LOR_D : Destination register
 * LOR_C : Constant register
 * LOR_K : Key register
 */
/* words/bytes(hex)/bits */

#define LOR_K0 LW_SE_LWPKA_CORE_LOR_K0_0 /* 128/0x200/4096 */
#define LOR_K1 LW_SE_LWPKA_CORE_LOR_K1_0 /* 64/0x100/2048 */
#define LOR_S0 LW_SE_LWPKA_CORE_LOR_S0_0 /* 128/0x200/4096 */
#define LOR_S1 LW_SE_LWPKA_CORE_LOR_S1_0 /* 128/0x200/4096 */
#define LOR_S2 LW_SE_LWPKA_CORE_LOR_S2_0 /* 32/0x80/1024 */
#define LOR_S3 LW_SE_LWPKA_CORE_LOR_S3_0 /* 32/0x80/1024 */
#define LOR_D0 LW_SE_LWPKA_CORE_LOR_D0_0 /* 64/0x100/2048 */
#define LOR_D1 LW_SE_LWPKA_CORE_LOR_D1_0 /* 128/0x200/4096 */
#define LOR_C0 LW_SE_LWPKA_CORE_LOR_C0_0 /* 128/0x200/4096 */
#define LOR_C1 LW_SE_LWPKA_CORE_LOR_C1_0 /* 32/0x80/1024 */
#define LOR_C2 LW_SE_LWPKA_CORE_LOR_C2_0 /* 32/0x80/1024 */

/* LOR register usage per operation class:
 *
 * CLASS   | MAXBITS | SRC   | DST | CONSTANT | VALUES
 * ++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * Normal  | 512     | S0,S1 | D1  | -        | -
 * Modular | 4096    | S0,S1 | D1  | C0       | -
 * RSA     | 4096    | S0    | D1  | C0       | K0
 * ECC     | 1024    | S0,S1 | D0, | C0,C1,   | K0,
 *                           | D1  | C2       | K1
 * Special | 512     | S0,S1 | D0  | C0,C1,   | K0,
 *                   | S2,S3 | D1  | C2       | K1
 * ++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

/* Core registers */

/* SW triggers LWPKA operation WITHOUT keyslot:
 * 1) Grab LWPKA mutex
 * 2) Ensure LWPKA is IDLE
 * 3) Soft reset LWPKA
 * 4) Setup primitive specific control with LW_CORE
 * 5) Transfer primitive operands to LW_CORE LOR
 * 6) Kick off operations without HW keyslot
 *    - Program primitive,radix, etc:
 *      CORE_OPERATION: <radix, primitive, start>
 * 7) Wait OP_DONE ack
 * 8) Transfer primitive outputs from LW_CORE LOR
 * 9) Scrub LOR
 *10) Release LWPKA mutex
 *
 * 1) Set START 1U in CORE_OPERATION register
 * 2) Keep reading STATUS register until PKA_STATUS bit is 0U
 */
#endif /* INCLUDED_TEGRA_LWPKA_H */
