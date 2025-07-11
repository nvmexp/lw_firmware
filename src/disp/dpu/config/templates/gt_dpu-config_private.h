// This file is automatically generated by chip-config - DO NOT EDIT!
//
// DPU app generated #defines such as IsG84(),
// DPUCFG_FEATURE_ENABLED_STATUS(), etc.
//
// Only for use within dpu applications.
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
// Also have DPUCFG_CHIP_ENABLED(), DPUCFG_FEATURE_ENABLED(), etc
// from {{$XXCONFIG_H_FILE}}.
//
// NOTE: these definitions are "flat" (ie they don't use some more general
//       DPUCFG_ENABLED(CHIP,X) form because the pre-processor would re-evaluate
//       the expansion of the item (gpu, feature, class, api).  For classes,
//       at least, this is a problem since we would end up with class number
//       instead of its name...

// hack: MSVC is not C99 compliant
#ifdef LW_WINDOWS
#define __func__ __FUNCTION__
#endif

// CHIP's
#define DPUCFG_CHIP_ENABLED_STATUS(W)   (DPUCFG_CHIP_##W ? {{$STATUS_OK}} : {{$STATUS_ERROR}})
#define DPUCFG_CHIP_ENABLED_OR_BAIL(W)                \
     do {                                           \
         if ( ! DPUCFG_CHIP_##W)                      \
         {                                          \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "DPU: %s: CHIP " #W " not enabled, bailing\n", __func__)); \
             return {{$STATUS_ERROR}};           \
         }                                          \
     } while (LW_FALSE)
#define DPUCFG_CHIP_ENABLED_OR_ASSERT_AND_BAIL(W)     \
     do {                                           \
         if ( ! DPUCFG_CHIP_##W)                      \
         {                                          \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "DPU: %s: CHIP " #W " not enabled, assert and bail\n", __func__)); \
             {{$ASSERT_DEFINE}}(DPUCFG_CHIP_##W);                        \
             return {{$STATUS_ERROR}};           \
         }                                          \
     } while (LW_FALSE)

// FEATURE's
#define DPUCFG_FEATURE_ENABLED_STATUS(W)   (DPUCFG_FEATURE_##W ? {{$STATUS_OK}} : {{$STATUS_ERROR}})
#define DPUCFG_FEATURE_ENABLED_OR_BAIL(W)            \
     do {                                           \
         if ( ! DPUCFG_FEATURE_##W)                  \
         {                                          \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "DPU: %s: FEATURE " #W " not enabled, bailing\n", __func__)); \
             return {{$STATUS_ERROR}};           \
         }                                          \
     } while (LW_FALSE)
#define DPUCFG_FEATURE_ENABLED_OR_ASSERT_AND_BAIL(W) \
     do {                                           \
         if ( ! DPUCFG_FEATURE_##W)                  \
         {                                          \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "DPU: %s: FEATURE " #W " not enabled, assert and bail\n", __func__)); \
             {{$ASSERT_DEFINE}}(DPUCFG_FEATURE_##W);          \
             return {{$STATUS_ERROR}};           \
         }                                          \
     } while (LW_FALSE)

// ENGINE's
#define DPUCFG_ENGINE_ENABLED_STATUS(W)   (DPUCFG_ENGINE_##W ? {{$STATUS_OK}} : {{$STATUS_ERROR}})
#define DPUCFG_ENGINE_ENABLED_OR_BAIL(W)             \
     do {                                           \
         if ( ! DPUCFG_ENGINE_##W)                   \
         {                                          \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "DPU: %s: ENGINE " #W " not enabled, bailing\n", __func__)); \
             return {{$STATUS_ERROR}};           \
         }                                          \
     } while (LW_FALSE)
#define DPUCFG_ENGINE_ENABLED_OR_ASSERT_AND_BAIL(W)  \
     do {                                           \
         if ( ! DPUCFG_ENGINE_##W)                   \
         {                                          \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "DPU: %s: ENGINE " #W " not enabled, assert and bail\n", __func__)); \
             {{$ASSERT_DEFINE}}(DPUCFG_ENGINE_##W);           \
             return {{$STATUS_ERROR}};           \
         }                                          \
     } while (LW_FALSE)

// CLASS's
#define DPUCFG_CLASS_ENABLED_STATUS(W)   (DPUCFG_CLASS_##W ? {{$STATUS_OK}} : {{$STATUS_ERROR}})
#define DPUCFG_CLASS_ENABLED_OR_BAIL(W)              \
     do {                                           \
         if ( ! DPUCFG_CLASS_##W)                    \
         {                                          \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "DPU: %s: CLASS " #W " not enabled, bailing\n", __func__)); \
             return {{$STATUS_ERROR}};           \
         }                                          \
     } while (LW_FALSE)
#define DPUCFG_CLASS_ENABLED_OR_ASSERT_AND_BAIL(W)   \
     do {                                           \
         if ( ! DPUCFG_CLASS_##W)                    \
         {                                          \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "DPU: %s: CLASS " #W " not enabled, assert and bail\n", __func__)); \
             {{$ASSERT_DEFINE}}(DPUCFG_CLASS_##W);            \
             return {{$STATUS_ERROR}};           \
         }                                          \
     } while (LW_FALSE)

// API's
#define DPUCFG_API_ENABLED_STATUS(W)   (DPUCFG_API_##W ? {{$STATUS_OK}} : {{$STATUS_ERROR}})
#define DPUCFG_API_ENABLED_OR_BAIL(W)                \
     do {                                           \
         if ( ! DPUCFG_API_##W)                      \
         {                                          \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "DPU: %s: API " #W " not enabled, bailing\n", __func__)); \
             return {{$STATUS_ERROR}};           \
         }                                          \
     } while (LW_FALSE)
#define DPUCFG_API_ENABLED_OR_ASSERT_AND_BAIL(W)     \
     do {                                           \
         if ( ! DPUCFG_API_##W)                      \
         {                                          \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "DPU: %s: API " #W " not enabled, assert and bail\n", __func__)); \
             {{$ASSERT_DEFINE}}(DPUCFG_API_##W);              \
             return {{$STATUS_ERROR}};           \
         }                                          \
     } while (LW_FALSE)


#endif  // G_{{$XXCFG}}_PRIVATE_H
