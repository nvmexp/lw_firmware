/*
 * Copyright (c) 2015-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */

#ifndef CCC_MAKEFILE_UPDATED

/* This is a temporary file until your subsystem updates the build files.
 *
 * When you do update the build makefiles, define CCC_MAKEFILE_UPDATED
 * or remove tegra_se.c from build files.
 *
 * These source file includes are used because updating subsystem
 * makefiles syncronously with CCC source changes is diffilwlt, partilwlarly
 * with GVS systems compiling CCC.
 *
 * Files split out of crypto_process.c:
 */
#include <crypto_process_aes_cipher.c>
#include <crypto_process_aes_mac.c>
#include <crypto_process_sha.c>
#include <crypto_process_random.c>

/* and from class layer files: */
#include <crypto_akey_proc.c>
#include <crypto_asig_proc.c>
#include <crypto_cipher_proc.c>
#include <crypto_derive_proc.c>
#include <crypto_mac_proc.c>
#include <crypto_md_proc.c>
#include <crypto_random_proc.c>

/* key slot set/clear functions for SE (AES)
 * and PKA (RSA) keyslots.
 */
#include <crypto_ks_se.c>
#include <crypto_ks_pka.c>

/* tegra_se.c is split into per engine files and no longer
 * compiled as after subsystems build makefiles are updated.
 */
#include <tegra_se_aes.c>
#include <tegra_se_mac.c>
#include <tegra_se_sha.c>
#include <tegra_se_pka0.c>
#include <tegra_se_rng.c>
#include <tegra_se_unwrap.c>

/* This is temporary until old subsystems update their Makefiles.
 *
 * After the code reorganization commit (Change-Id:
 * I632b0c63b331ed6e4c9f59ea7b4e6d79e698316d) which shares SE device
 * code in T19X/T18X and T23X files => a new file "tegra_se_gen.c"
 * contains common code from tegra_se.c and tegra_se_kac.c and it
 * needs to be added to all subsystem makefiles.
 *
 * Until that happens for the old subsystems, just include the
 * shared code in tegra_se.c to allow GVS tests to pass for
 * the old subsystems.
 *
 * Once old makefiles have been updated => remove this C file
 * inclusion kludge !!!
 *
 * New subsystems using tegra_se_kac.c will not have this backwards
 * compatibility kludge, they must add tegra_se_gen.c to makefiles
 * immediatelly.
 */

#include <tegra_se_gen.c>
#include <crypto_classcall.c>

#include <tegra_cdev_ecall.c>
#include <crypto_process_user_call.c>

#include <context_mem.c>

#endif /* CCC_MAKEFILE_UPDATED */
