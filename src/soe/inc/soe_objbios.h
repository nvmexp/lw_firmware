#ifndef SOE_OBJBIOS_H_INCLUDED
#define SOE_OBJBIOS_H_INCLUDED

/*!
 * @file    soe_objbios.h 
 * @copydoc soe_objbios.c
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "config/g_bios_hal.h"

/* ------------------------ Types Definitions ------------------------------ */

// BIOS Image
typedef struct
{
    // Size of the image.
    LwU32 size;

    // pointer to the BIOS image.
    LwU8* pImage;
} LWSWITCH_BIOS_IMAGE;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
extern BIOS_HAL_IFACES BiosHal;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */

/* ------------------------ Public Functions ------------------------------- */
void constructBios(void) GCC_ATTRIB_SECTION("imem_init", "constructBios");
#endif 
// SOE_OBJBIOS_H_INCLUDED

