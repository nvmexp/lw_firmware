#define IN_ADA_BODY
#include "exceptions_lw.h"

void
  GNAT_LINKER_SECTION(".imem_pub")
exception_handler_selwre_hang_pub(void) {{

    while (true) {
      {}
    }
  }
  return;
}

