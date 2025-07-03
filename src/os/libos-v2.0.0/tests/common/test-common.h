#pragma once

#include "libos_log.h"

#define NUM_(x) #x
#define NUM(x)  NUM_(x)
#define LOG_AND_VALIDATE(LOG, x)                                                                             \
    {                                                                                                        \
        const static __attribute__((section(".logging"))) char statement[] =                                 \
            __FILE__ ":" NUM(__LINE__) " " #x;                                                               \
                                                                                                             \
        LOG(LOG_LEVEL_INFO, "\t\tExelwting: %s\n", statement);                                               \
        if (!(x))                                                                                            \
        {                                                                                                    \
            LOG(LOG_LEVEL_INFO, "FAILED! %s\n", statement);                                                  \
        }                                                                                                    \
    }

#define LOG_AND_VALIDATE_EQUAL(LOG, status, expected, x)                                                     \
    {                                                                                                        \
        const static __attribute__((section(".logging"))) char statement[] =                                 \
            __FILE__ ":" NUM(__LINE__) " " #x;                                                               \
        LOG(LOG_LEVEL_INFO, "\t\tExelwting: %s\n", statement);                                               \
        status = (x);                                                                                        \
        if (status != expected)                                                                              \
        {                                                                                                    \
            LOG(LOG_LEVEL_INFO, "FAILED! %s\n", statement);                                                  \
            LOG(LOG_LEVEL_INFO, ":\t\t%d, got %d\n", expected, status);                                      \
        }                                                                                                    \
    }
