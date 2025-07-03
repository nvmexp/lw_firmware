/*
 * Copyright (c) 2018-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_CRYPTO_SOC_H
#define INCLUDED_CRYPTO_SOC_H

#define CCC_SOC_ON_CHIP         0x0000000U
#define CCC_SOC_ON_FPGA         0x0100000U
#define CCC_SOC_ON_VDK          0x0200000U
#define CCC_SOC_ON_TEGRASIM     0x0400000U

#define CCC_SOC_ON_VALUE_SHIFT  20U
#define CCC_SOC_ON_VALUE_MASK   0x0F00000U

#define CCC_SOC_FAMILY_T21X_ID    0x010210U
#define CCC_SOC_FAMILY_T18X_ID    0x020180U
#define CCC_SOC_FAMILY_T19X_ID    0x030190U
#define CCC_SOC_FAMILY_T23X_ID    0x040230U

/* CCC on SoC family.
 *
 * Override with a different CCC_SOC_ON_xxx if the default is not correct.
 */
#ifndef CCC_SOC_FAMILY_T21X
#define CCC_SOC_FAMILY_T21X    (CCC_SOC_FAMILY_T21X_ID | CCC_SOC_ON_CHIP)
#endif
#ifndef CCC_SOC_FAMILY_T18X
#define CCC_SOC_FAMILY_T18X    (CCC_SOC_FAMILY_T18X_ID | CCC_SOC_ON_CHIP)
#endif
#ifndef CCC_SOC_FAMILY_T19X
#define CCC_SOC_FAMILY_T19X    (CCC_SOC_FAMILY_T19X_ID | CCC_SOC_ON_CHIP)
#endif
#ifndef CCC_SOC_FAMILY_T23X
#define CCC_SOC_FAMILY_T23X    (CCC_SOC_FAMILY_T23X_ID | CCC_SOC_ON_CHIP)
#endif

/* Configure for T18X if no knowledge of the target SOC */
#define CCC_SOC_FAMILY_DEFAULT CCC_SOC_FAMILY_T18X

#if !defined(CCC_SOC_FAMILY_TYPE)
/* Map the old HAVE_FPGA to CCC_SOC_FAMILY_T19X if soc_family not defined.
 *
 * HAVE_FPGA should not be used anymore, please define CCC_SOC_FAMILY_TYPE
 * to one of the CCC_SOC_FAMILY_T<*> soc_family names in crypto config file or
 * make file.
 */
# if (defined(HAVE_FPGA) && HAVE_FPGA) || (defined(HAVE_ELLIPTIC_20) && HAVE_ELLIPTIC_20)
#   define CCC_SOC_FAMILY_TYPE CCC_SOC_FAMILY_T19X
# endif
#endif /* !defined(CCC_SOC_FAMILY_TYPE) */

#if !defined(CCC_SOC_FAMILY_TYPE)
#define CCC_SOC_FAMILY_TYPE CCC_SOC_FAMILY_DEFAULT
#endif /* !defined(CCC_SOC_FAMILY_TYPE) */

/* Current CCC on HW, FPGA, VDK... For temporary WARs (mainly).
 */
#define CCC_SOC_FAMILY_ON(x) (((CCC_SOC_FAMILY_TYPE) & CCC_SOC_ON_VALUE_MASK) == (x))

#define CCC_SOC_FAMILY_ID(x) (((x) & 0x0FF0U) >> 4U)
#define CCC_SOC_FAMILY_IS(x) (CCC_SOC_FAMILY_ID(CCC_SOC_FAMILY_TYPE) == CCC_SOC_FAMILY_ID(x))

/* Check that CCC supports the soc family id */
#if ((CCC_SOC_FAMILY_ID(CCC_SOC_FAMILY_TYPE) != CCC_SOC_FAMILY_ID(CCC_SOC_FAMILY_T21X)) && \
     (CCC_SOC_FAMILY_ID(CCC_SOC_FAMILY_TYPE) != CCC_SOC_FAMILY_ID(CCC_SOC_FAMILY_T18X)) && \
     (CCC_SOC_FAMILY_ID(CCC_SOC_FAMILY_TYPE) != CCC_SOC_FAMILY_ID(CCC_SOC_FAMILY_T19X)) && \
     (CCC_SOC_FAMILY_ID(CCC_SOC_FAMILY_TYPE) != CCC_SOC_FAMILY_ID(CCC_SOC_FAMILY_T23X)))
#error "CCC does not support specified SoC family"
#endif

#ifndef CCC_SOC_WITH_PKA1
/* These SoC families contain the PKA1/RNG1 HW devices
 */
#define CCC_SOC_WITH_PKA1 \
	(CCC_SOC_FAMILY_IS(CCC_SOC_FAMILY_T19X) || CCC_SOC_FAMILY_IS(CCC_SOC_FAMILY_T18X))
#endif

/* SoC family macros for setting new SoC features
 * (Would be nice if there would be a ID for the SE engine)
 */
#if CCC_SOC_FAMILY_IS(CCC_SOC_FAMILY_T23X)
#define HAVE_SOC_FEATURE_KAC		 1
#define HAVE_SOC_FEATURE_XTS_SUBKEY_SLOT 1
#define HAVE_SOC_FEATURE_T23X_MMIO_RANGE 1
#define HAVE_SOC_FEATURE_T23X_ENGINES	 1
#endif /* CCC_SOC_FAMILY_IS(CCC_SOC_FAMILY_T23X) */

#ifndef HAVE_SOC_FEATURE_T23X_ENGINES
#define HAVE_SOC_FEATURE_T23X_ENGINES 0
#endif

#ifndef CCC_SOC_WITH_LWPKA
#define CCC_SOC_WITH_LWPKA HAVE_SOC_FEATURE_T23X_ENGINES
#endif

#ifndef HAVE_SOC_FEATURE_T23X_MMIO_RANGE
#define HAVE_SOC_FEATURE_T23X_MMIO_RANGE 0
#endif

#ifndef HAVE_SOC_FEATURE_KAC
#define HAVE_SOC_FEATURE_KAC 0
#endif

#ifndef HAVE_SOC_FEATURE_XTS_SUBKEY_SLOT
#define HAVE_SOC_FEATURE_XTS_SUBKEY_SLOT 0
#endif

#endif /* INCLUDED_CRYPTO_SOC_H */
