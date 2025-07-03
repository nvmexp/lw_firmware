#ifndef SOE_DEVICE_H_INCLUDED
#define SOE_DEVICE_H_INCLUDED

/*!
 * @file    soe_objlwlink.h 
 * @copydoc soe_objlwlink.c
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "soe_objbios.h"

/* ------------------------ Types Definitions ------------------------------ */

//
// common device information
//
struct lwswitch_device
{
    // Full BIOS image
    LWSWITCH_BIOS_IMAGE                  biosImage;
};

typedef struct lwswitch_device           lwswitch_device;

/* ------------------------ Global Variables ------------------------------- */
extern lwswitch_device device;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */

#endif // SOE_DEVICE_H_INCLUDED

