/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DEBUG_H
#define DEBUG_H

#include <liblwriscv/libc.h>
#include <liblwriscv/print.h>

#define DEBUG_ENABLE 1

#if(DEBUG_ENABLE) // defined in utils.h
    #define TRACE_NEWLINE() printf("\n")
    #define TRACE_NN(...) printf(__VA_ARGS__)
    #define TRACE(...) TRACE_NN(__VA_ARGS__); TRACE_NEWLINE()
    #define TRACE_START(...) printf("[START]: "); TRACE(__VA_ARGS__)
    #define TRACE_END(...) printf("[END]: "); TRACE(__VA_ARGS__)

    #define DUMP_MEM(name, buf, len) do {\
            if(buf != NULL) {\
                TRACE_NN("%s (%d):", name, len);\
                for(uint32_t i = 0; i < (len); i++) { \
                    TRACE_NN(" %02X", ((uint8_t*)buf)[i]);\
                } \
                TRACE_NEWLINE();\
            } \
        } while(false)

    #define DUMP_MEM32(name, buf, len) do {\
            if(buf != NULL) {\
                TRACE_NN("%s:", name);\
                for(uint32_t i = 0; i < (len); i++) { \
                    TRACE_NN(" %08X", ((uint32_t*)buf)[i]);\
                } \
                TRACE_NEWLINE();\
            } \
        } while(false)
#else
    #define TRACE_NEWLINE()
    #define TRACE_NN(...)
    #define TRACE(...)
    #define TRACE_START(...)
    #define TRACE_END(...)
    #define DUMP_MEM(name, buf, len)
    #define DUMP_MEM32(name, buf, len)
#endif // DEBUG_ENABLE


#endif  // DEBUG_H
