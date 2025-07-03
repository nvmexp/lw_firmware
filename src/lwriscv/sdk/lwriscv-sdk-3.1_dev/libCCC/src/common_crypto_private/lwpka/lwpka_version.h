/*
 * Copyright (c) 2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

/* LWPKA version numbers for T23X and newer
 */
#ifndef INCLUDED_LWPKA_VERSION_H
#define INCLUDED_LWPKA_VERSION_H

#ifdef DOXYGEN
#include <ccc_doxygen.h>
#endif

#ifndef HAVE_LWPKA_DO_SETUP
/* Subsystem can prevent version checks and any setup of LWPKA
 * engine by defining HAVE_LWPKA_DO_SETUP 0
 *
 * They are then fully responsible for any current and future setup
 * required by the engine.
 */
#define HAVE_LWPKA_DO_SETUP 1
#endif

#define LWPKA_VERSION_T23X      0x00010001U
#define LWPKA_VERSION_LWPKA11   0x01010101U
#define LWPKA_VERSION_AD10X     0x04010401U

static inline bool m_lwpka_version_supported(uint32_t version)
{
        bool supported = false;

        switch (version) {
#if HAVE_LWPKA11_SUPPORT
        case LWPKA_VERSION_LWPKA11:
#endif
        case LWPKA_VERSION_T23X:
        case LWPKA_VERSION_AD10X:
                supported = true;
                break;
        default:
                supported = false;
                break;
        }
        return supported;
}

#define LWPKA_VERSION_SUPPORTED(lwersion) m_lwpka_version_supported(lwersion)

/* XXX Fix the HW header files to contain proper defines for new
 * fields in CTRL_BUILD_CONFIG (Use DRF header values once defined
 * there)
 */
#define BCONF_MASK_LWPKA11_EC_KS 0x0000000200U
#define BCONF_MASK_HAS_SAFETY    0x0000000100U
#define BCONF_MASK_KSLT_SAFETY   0x000000000LW
#define BCONF_MASK_DMEM_SAFETY   0x0000000003U

#endif /* INCLUDED_LWPKA_VERSION_H */
