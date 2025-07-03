#define IN_ADA_BODY
#include "dev_master.h"

typedef struct _dev_master__lw_pmc_boot_42_register {
  unsigned_32 _pad1 : 8;
  unsigned_32 minor_extended_revision : 4;
  unsigned_32 minor_revision : 4;
  unsigned_32 major_revision : 4;
  unsigned_32 chip_id : 9;
  unsigned_32 rsvd : 3;
} GNAT_PACKED dev_master__lw_pmc_boot_42_register;
const lw_types__reg_addr_types__bar0_addr dev_master__lw_pmc_boot_42_addr = 2560U;
