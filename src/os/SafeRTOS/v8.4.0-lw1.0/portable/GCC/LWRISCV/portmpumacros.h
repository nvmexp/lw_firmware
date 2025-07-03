/*
        Copyright (C)2006 onward WITTENSTEIN aerospace & simulation limited. All rights reserved.

        This file is part of the SafeRTOS product, see projdefs.h for version number
        information.

        SafeRTOS is distributed exclusively by WITTENSTEIN high integrity systems,
        and is subject to the terms of the License granted to your organization,
        including its warranties and limitations on use and distribution. It cannot be
        copied or reproduced in any way except as permitted by the License.

        Licenses authorize use by processor, compiler, business unit, and product.

        WITTENSTEIN high integrity systems is a trading name of WITTENSTEIN
        aerospace & simulation ltd, Registered Office: Brown's Court, Long Ashton
        Business Park, Yanley Lane, Long Ashton, Bristol, BS41 9LB, UK.
        Tel: +44 (0) 1275 395 600, fax: +44 (0) 1275 393 630.
        E-mail: info@HighIntegritySystems.com

        http://www.HighIntegritySystems.com
*/

#ifndef PORTMPUMACROS_H
#define PORTMPUMACROS_H

/* MPU region actual byte size limits. */
#define portmpuSMALLEST_REGION_SIZE_ACTUAL              ( 0x1000ULL )
#define portmpuLARGEST_REGION_SIZE_ACTUAL               ( 0xFFFFFFFFF000ULL )

/* Global MPU regions that will not change in context switch */
#define portmpuPRIVILEGED_KERNEL_CODE_REGION            ( 10UL )
#define portmpuPRIVILEGED_KERNEL_DATA_REGION            ( 11UL )
#define portmpuUNPRIVILEGED_GLOBAL_CODE_REGION          ( 12UL )
#define portmpuUNPRIVILEGED_GLOBAL_RODATA_REGION        ( 13UL )
#define portmpuUNPRIVILEGED_GLOBAL_DATA_REGION          ( 14UL )
#define portmpuUNPRIVILEGED_GLOBAL_BSS_REGION           ( 15UL )
#define portmpuPRIVILEGED_HEAP_STACK_REGION             ( 16UL )
#define portmpuPRIVILEGED_PRIV_REGION                   ( 17UL )
#define portmpuPRIVILEGED_KERNEL_CODE_HUB_REGION        ( 18UL )
#define portmpuPRIVILEGED_KERNEL_DATA_HUB_REGION        ( 19UL )
#define portmpuPRIVILEGED_BOOT_REGION                   ( 20UL )
#define portmpuPRIVILEGED_BOOT_MBARE_REGION             ( 21UL )
#define portmpuTASK_DYNAMIC_REGION_END                  ( 63UL )

/* Task specific MPU regions */
#define portmpuTASK_CODE_REGION                     ( 0UL )
#define portmpuTASK_RODATA_REGION                   ( 1UL )
#define portmpuTASK_DATA_REGION                     ( 2UL )
#define portmpuTASK_BSS_REGION                      ( 3UL )
#define portmpuTASK_HEAP_META_REGION                ( 4UL )
#define portmpuTASK_HEAP_DATA_REGION                ( 5UL )
#define portmpuSTACK_REGION                         ( 6UL )
#define portmpuADDITIONAL_REGION_FIRST              ( 7UL )
#define portmpuADDITIONAL_REGION_LAST               ( 9UL )
#define portmpuADDITIONAL_REGION_NUM                \
                ( ( portmpuADDITIONAL_REGION_LAST - portmpuADDITIONAL_REGION_FIRST ) + 1 )
#define portmpuCONFIGURABLE_REGION_FIRST            ( 0UL )
#define portmpuCONFIGURABLE_REGION_LAST             ( 9UL )
#define portmpuCONFIGURABLE_REGION_NUM              \
                ( ( portmpuCONFIGURABLE_REGION_LAST - portmpuCONFIGURABLE_REGION_FIRST ) + 1 )
#define portmpuTOTAL_REGION_NUM                     ( portmpuCONFIGURABLE_REGION_NUM )
#define portmpuTASK_REGION_NUM                      ( portmpuTOTAL_REGION_NUM )

/*
 * Constants used to configure the region attributes.
 */

#define portmpuREGION_VALID                              ( 0x0000000000000001ULL )

#define portmpuREGION_USER_MODE_READ_ONLY                ( 0x0000000000000001ULL )
#define portmpuREGION_USER_MODE_READ_WRITE               ( 0x0000000000000003ULL )
#define portmpuREGION_USER_MODE_READ_EXELWTE             ( 0x0000000000000005ULL )
#define portmpuREGION_USER_MODE_READ_WRITE_EXELWTE       ( 0x0000000000000007ULL )

#define portmpuREGION_SUPERVISOR_MODE_READ_ONLY          ( 0x0000000000000008ULL )
#define portmpuREGION_SUPERVISOR_MODE_READ_WRITE         ( 0x0000000000000018ULL )
#define portmpuREGION_SUPERVISOR_MODE_READ_EXELWTE       ( 0x0000000000000028ULL )
#define portmpuREGION_SUPERVISOR_MODE_READ_WRITE_EXELWTE ( 0x0000000000000038ULL )

#define portmpuREGION_KERNEL_MODE_READ_ONLY              portmpuREGION_SUPERVISOR_MODE_READ_ONLY
#define portmpuREGION_KERNEL_MODE_READ_WRITE             portmpuREGION_SUPERVISOR_MODE_READ_WRITE
#define portmpuREGION_KERNEL_MODE_READ_EXELWTE           portmpuREGION_SUPERVISOR_MODE_READ_EXELWTE
#define portmpuREGION_KERNEL_MODE_READ_WRITE_EXELWTE     portmpuREGION_SUPERVISOR_MODE_READ_WRITE_EXELWTE

/* generic modes = USER + KERNEL */
#define portmpuREGION_READ_ONLY                          ( portmpuREGION_USER_MODE_READ_ONLY |  portmpuREGION_KERNEL_MODE_READ_ONLY )
#define portmpuREGION_READ_WRITE                         ( portmpuREGION_USER_MODE_READ_WRITE | portmpuREGION_KERNEL_MODE_READ_WRITE )
#define portmpuREGION_READ_EXELWTE                       ( portmpuREGION_USER_MODE_READ_EXELWTE | portmpuREGION_KERNEL_MODE_READ_EXELWTE )
#define portmpuREGION_READ_WRITE_EXELWTE                 ( portmpuREGION_USER_MODE_READ_WRITE_EXELWTE | portmpuREGION_KERNEL_MODE_READ_WRITE_EXELWTE )

#define portmpuREGION_CACHEABLE                          ( 0x0000000000040000ULL )

/* Attributes for every task stack region. */
#define portmpuTASK_STACK_ATTRIBUTES    ( portmpuREGION_READ_WRITE | portmpuREGION_CACHEABLE )

#endif /* PORTMPUMACROS_H */
