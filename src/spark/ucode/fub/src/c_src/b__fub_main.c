#define IN_ADA_BODY
#include "b__fub_main.h"

void adainit(void) {
  {}
  return;
}
extern void _ada_fub_main(void);
integer main(void) {
  adainit();
  _ada_fub_main();
  return (ada_main__gnat_exit_status);
}

