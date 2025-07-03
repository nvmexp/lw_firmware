/*
 * Copyright (c) 2016-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */

/**
 * @file   crypto_ops.h
 * @author Juki <jvirtanen@lwpu.com>
 * @date   2016
 *
 * Definitions for common crypto code.
 *
 * These are used by CCC internally but also required
 * when CCC is linked to the subsystem as a library.
 */
#ifndef INCLUDED_CRYPTO_OPS_H
#define INCLUDED_CRYPTO_OPS_H

#ifndef CCC_SOC_FAMILY_DEFAULT
/* Some subsystems do not include the CCC config file before including
 * other CCC headers; normally crypto_ops.h. So include the config
 * here unless the soc family is defined.
 */
#include <crypto_system_config.h>
#endif

#include <include/tegra_se.h>
#include <crypto_context.h>
#include <crypto_ops_intern.h>
#include <crypto_lib_api.h>

/***************************************/

#ifndef CCC_SYSTEM_MAP_DEVICE_ARGUMENT_NO_CASTING
/* By default, cast enum argument to uint32_t and
 * cast addresss to void ** for backwards
 * compatibility.
 *
 * If this is non-zero: do not cast those, assume
 * subsystem knows the argument types (crypto_api.h)
 */
#define CCC_SYSTEM_MAP_DEVICE_ARGUMENT_NO_CASTING 0
#endif

#if CCC_SYSTEM_MAP_DEVICE_ARGUMENT_NO_CASTING
/* Need to declare the function prototype for this case here.
 *
 * It can not be declared in the config file in this case because
 * the se_cdev_id_t type is not likely/necessarily known in the config file.
 *
 * Both argument types are now correct so no type casting is required.
 */
status_t SYSTEM_MAP_DEVICE(se_cdev_id_t device_id, uint8_t **base_p);
#endif /* CCC_SYSTEM_MAP_DEVICE_ARGUMENT_NO_CASTING */

/***************************************/
#endif /* INCLUDED_CRYPTO_OPS_H */
