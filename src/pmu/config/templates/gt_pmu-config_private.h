// This file is automatically generated by chip-config - DO NOT EDIT!
//
// PMU app generated #defines such as IsG84(),
// PMUCFG_FEATURE_ENABLED_STATUS(), etc.
//
// Only for use within pmu applications.
//
// Profile:  {{$PROFILE}}
// Template: {{$TEMPLATE_FILE}}
//
// Chips:    {{ CHIP_LIST() }}
//

#ifndef G_{{$XXCFG}}_PRIVATE_H
#define G_{{$XXCFG}}_PRIVATE_H

//
// Macros to help with enabling or disabling code based on whether
// a feature (or chip or engine or ...) is enabled or not.
// Also have PMUCFG_CHIP_ENABLED(), PMUCFG_FEATURE_ENABLED(), etc
// from {{$XXCONFIG_H_FILE}}.
//
// NOTE: these definitions are "flat" (ie they don't use some more general
//       PMUCFG_ENABLED(CHIP,X) form because the pre-processor would re-evaluate
//       the expansion of the item (gpu, feature, class, api).  For classes,
//       at least, this is a problem since we would end up with class number
//       instead of its name...

// hack: MSVC is not C99 compliant
#ifdef LW_WINDOWS
#define __func__ __FUNCTION__
#endif

// CHIP's
#define PMUCFG_CHIP_ENABLED_STATUS(W)   (PMUCFG_CHIP_##W ? {{$STATUS_OK}} : {{$STATUS_ERROR}})
#define PMUCFG_CHIP_ENABLED_OR_BAIL(W)             \
     do {                                          \
         if (0 == PMUCFG_CHIP_##W)                 \
         {                                         \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "PMU: %s: CHIP " #W " not enabled, bailing\n", __func__)); \
             return {{$STATUS_ERROR}};             \
         }                                         \
     } while(LW_FALSE)
#define PMUCFG_CHIP_ENABLED_OR_ASSERT_AND_BAIL(W)  \
     do {                                          \
         if (0 == PMUCFG_CHIP_##W)                 \
         {                                         \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "PMU: %s: CHIP " #W " not enabled, assert and bail\n", __func__)); \
             {{$ASSERT}}(PMUCFG_CHIP_##W);         \
             return {{$STATUS_ERROR}};             \
         }                                         \
     } while(LW_FALSE)
#define PMUCFG_CHIP_ENABLED_OR_GOTO(W,L) \
    do {                                 \
        if (0 == PMUCFG_CHIP_##W)        \
        {                                \
            goto L;                      \
        }                                \
    } while (LW_FALSE)
#define PMUCFG_CHIP_ENABLED_OR_BREAK(W)  \
    {                                    \
        if (0 == PMUCFG_CHIP_##W)        \
        {                                \
            break;                       \
        }                                \
    }

// FEATURE's
#define PMUCFG_FEATURE_ENABLED_STATUS(W)   (PMUCFG_FEATURE_##W ? {{$STATUS_OK}} : {{$STATUS_ERROR}})
#define PMUCFG_FEATURE_ENABLED_OR_BAIL(W)            \
     do {                                            \
         if (0 == PMUCFG_FEATURE_##W)                \
         {                                           \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "PMU: %s: FEATURE " #W " not enabled, bailing\n", __func__)); \
             return {{$STATUS_ERROR}};               \
         }                                           \
     } while(LW_FALSE)
#define PMUCFG_FEATURE_ENABLED_OR_ASSERT_AND_BAIL(W) \
     do {                                            \
         if (0 == PMUCFG_FEATURE_##W)                \
         {                                           \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "PMU: %s: FEATURE " #W " not enabled, assert and bail\n", __func__)); \
             {{$ASSERT}}(PMUCFG_FEATURE_##W);        \
             return {{$STATUS_ERROR}};               \
         }                                           \
     } while(LW_FALSE)
#define PMUCFG_FEATURE_ENABLED_OR_GOTO(W,L)  \
    do {                                     \
        if (0 == PMUCFG_FEATURE_##W)         \
        {                                    \
            goto L;                          \
        }                                    \
    } while (LW_FALSE)
#define PMUCFG_FEATURE_ENABLED_OR_BREAK(W)   \
    {                                        \
        if (0 == PMUCFG_FEATURE_##W)         \
        {                                    \
            break;                           \
        }                                    \
    }

// ENGINE's
#define PMUCFG_ENGINE_ENABLED_STATUS(W)   (PMUCFG_ENGINE_##W ? {{$STATUS_OK}} : {{$STATUS_ERROR}})
#define PMUCFG_ENGINE_ENABLED_OR_BAIL(W)            \
     do {                                           \
         if (0 == PMUCFG_ENGINE_##W)                \
         {                                          \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "PMU: %s: ENGINE " #W " not enabled, bailing\n", __func__)); \
             return {{$STATUS_ERROR}};              \
         }                                          \
     } while(LW_FALSE)
#define PMUCFG_ENGINE_ENABLED_OR_ASSERT_AND_BAIL(W) \
     do {                                           \
         if (0 == PMUCFG_ENGINE_##W)                \
         {                                          \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "PMU: %s: ENGINE " #W " not enabled, assert and bail\n", __func__)); \
             {{$ASSERT}}(PMUCFG_ENGINE_##W);        \
             return {{$STATUS_ERROR}};              \
         }                                          \
     } while(LW_FALSE)
#define PMUCFG_ENGINE_ENABLED_OR_GOTO(W,L)  \
    do {                                    \
        if (0 == PMUCFG_ENGINE_##W)         \
        {                                   \
            goto L;                         \
        }                                   \
    } while (LW_FALSE)
#define PMUCFG_ENGINE_ENABLED_OR_BREAK(W)   \
    {                                       \
        if (0 == PMUCFG_ENGINE_##W)         \
        {                                   \
            break;                          \
        }                                   \
    }

// CLASS's
#define PMUCFG_CLASS_ENABLED_STATUS(W)   (PMUCFG_CLASS_##W ? {{$STATUS_OK}} : {{$STATUS_ERROR}})
#define PMUCFG_CLASS_ENABLED_OR_BAIL(W)             \
     do {                                           \
         if (0 == PMUCFG_CLASS_##W)                 \
         {                                          \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "PMU: %s: CLASS " #W " not enabled, bailing\n", __func__)); \
             return {{$STATUS_ERROR}};              \
         }                                          \
     } while(LW_FALSE)
#define PMUCFG_CLASS_ENABLED_OR_ASSERT_AND_BAIL(W)  \
     do {                                           \
         if (0 == PMUCFG_CLASS_##W)                 \
         {                                          \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "PMU: %s: CLASS " #W " not enabled, assert and bail\n", __func__)); \
             {{$ASSERT}}(PMUCFG_CLASS_##W);         \
             return {{$STATUS_ERROR}};              \
         }                                          \
     } while(LW_FALSE)
#define PMUCFG_CLASS_ENABLED_OR_GOTO(W,L)  \
    do {                                   \
        if (0 == PMUCFG_CLASS_##W)         \
        {                                  \
            goto L;                        \
        }                                  \
    } while (LW_FALSE)
#define PMUCFG_CLASS_ENABLED_OR_BREAK(W)   \
    {                                      \
        if (0 == PMUCFG_CLASS_##W)         \
        {                                  \
            break;                         \
        }                                  \
    }

// API's
#define PMUCFG_API_ENABLED_STATUS(W)   (PMUCFG_API_##W ? {{$STATUS_OK}} : {{$STATUS_ERROR}})
#define PMUCFG_API_ENABLED_OR_BAIL(W)               \
     do {                                           \
         if (0 == PMUCFG_API_##W)                   \
         {                                          \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "PMU: %s: API " #W " not enabled, bailing\n", __func__)); \
             return {{$STATUS_ERROR}};              \
         }                                          \
     } while(LW_FALSE)
#define PMUCFG_API_ENABLED_OR_ASSERT_AND_BAIL(W)    \
     do {                                           \
         if (0 == PMUCFG_API_##W)                   \
         {                                          \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "PMU: %s: API " #W " not enabled, assert and bail\n", __func__)); \
             {{$ASSERT}}(PMUCFG_API_##W);           \
             return {{$STATUS_ERROR}};              \
         }                                          \
     } while(LW_FALSE)
#define PMUCFG_API_ENABLED_OR_GOTO(W,L)  \
    do {                                 \
        if (0 == PMUCFG_API_##W)         \
        {                                \
            goto L;                      \
        }                                \
    } while (LW_FALSE)
#define PMUCFG_API_ENABLED_OR_BREAK(W)   \
    {                                    \
        if (0 == PMUCFG_API_##W)         \
        {                                \
            break;                       \
        }                                \
    }


#endif  // G_{{$XXCFG}}_PRIVATE_H
