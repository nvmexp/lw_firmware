
#include "interface.h"
#include "fbflcn_defines.h"


const volatile FBFLCN_INIT_INTERFACE fbflcnInitIface =
        {
                /* .version                     = */ 1,
                /* .structSize                  = */ sizeof(FBFLCN_INIT_INTERFACE),
                /* .appVersion                  = */ FB_INIT_IFACE_APPVERSION_VALUE,
                /* .appFeatures                 = */ 0,
        };


typedef struct
{
APPLICATION_INTERFACE_HEADER header;
APPLICATION_INTERFACE_ENTRY  entry;
} APPLICATION_INTERFACE_TABLE;

// A GFW script looks for "__ifdata_header" in the ELF symbol table to locate the Application Interface Table
const volatile APPLICATION_INTERFACE_TABLE _ifdata_header =
        {
                .header = {
                        .version    = 1,
                        .headerSize = sizeof(struct APPLICATION_INTERFACE_HEADER),
                        .entrySize  = sizeof(struct APPLICATION_INTERFACE_ENTRY),
                        .entryCount = 1,
                },
                .entry  = {
                        .id  = APPLICATION_INTERFACE_ENTRY_ID_FB_INIT,
                        .ptr = (LwU32) &fbflcnInitIface,
                },
        };


