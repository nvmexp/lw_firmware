
/* ------------------------ System Includes -------------------------------- */
#include "soesw.h"
#include "soe_bar0.h"
#include "soe_device.h"
#include "spidev.h"
#include "soe_objcore.h"
#include "dmemovl.h"
#include "soe_objhal.h"
/* ------------------------ Application Includes --------------------------- */
//#include "config/g_bios_hal.h"
/* ------------------------ Global ----------------------------------------- */
BIOS_HAL_IFACES BiosHal;
struct lwswitch_device device;
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
void
constructBios(void)
{
    IfaceSetup->biosHalIfacesSetupFn(&BiosHal);
}

