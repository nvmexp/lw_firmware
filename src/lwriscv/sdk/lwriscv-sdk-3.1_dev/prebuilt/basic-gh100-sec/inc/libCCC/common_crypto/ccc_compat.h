/*
 * Copyright (c) 2020-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

/* Collection of CCC backwards compatibility defines and setting
 * for clients using CCC internal apis directly (like engine layer calls).
 *
 * After subsystem has been updated this can be deactivated.
 */
#ifndef INCLUDED_CCC_COMPAT_H
#define INCLUDED_CCC_COMPAT_H

#ifndef CCC_COMPAT_MODE
#define CCC_COMPAT_MODE 1
#endif

#if CCC_COMPAT_MODE

/* struct se_engine_aes_context contained unnamed structures.
 *
 * Commits naming such structures may cause issues for subsystems
 * which use the internal CCC apis directly bypassing the CCC api layer
 * (for example calling engine layer functions directly) will no longer
 * compile until such subsystems are modified to use the new field names.
 *
 * In order to pass GVS with such systems, some compatibility macros
 * are used during transition.  these can be disabled once the CCC
 * code is merged and subsystems updated to CCC TOT.
 */
#define AES_CTR_MODE_NAME_COMPATIBILITY

#define tegra_nc_region_get_length(domain, kva, remain, clen_p)		\
	ccc_nc_region_get_length(domain, kva, remain, clen_p)

#define tegra_select_device_engine(dev_id, eng_p, eclass)	\
	ccc_select_device_engine(dev_id, eng_p, eclass)

#define tegra_select_engine(eng_p, eclass, ehint)	\
	ccc_select_engine(eng_p, eclass, ehint)

#define tegra_ec_get_lwrve(lwrve_id) \
	ccc_ec_get_lwrve(lwrve_id)

#endif /* CCC_COMPAT_MODE */

#endif /* INCLUDED_CCC_COMPAT_H */
