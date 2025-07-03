#ifndef _STDLIB_H_
#define _STDLIB_H_


// Falcon instrinsics file provided by falcon tool chain.
#include <falcon-intrinsics.h>

// Falcon intrinsics file containing functions not provided by falcon tool chain.
#include "falcon-intrinsics-for-ccg.h"
#include "fub.h"
#include "fubutils.h"

// Defining our own custom Last chance handler.
#define GNAT_LAST_CHANCE_HANDLER(a, b) __lwstom_gnat_last_chance_handler()
extern void __lwstom_gnat_last_chance_handler(void) ATTR_OVLY(".imem_pub");

#endif // STDLIB_H
