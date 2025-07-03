#define IN_ADA_BODY
#include "utility.h"

void
  GNAT_LINKER_SECTION(".imem_pub")
__lwstom_gnat_last_chance_handler(void) {
  falc_halt();
  return;
}
void
  GNAT_LINKER_SECTION(".imem_pub")
utility__initialize_to_please_flow_analysis(_fatptr_UNCarray buf_all) {
  {}
  return;
}

