//
// FBFALCON app generated #defines such as IsG84(),
// FBFALCONCFG_FEATURE_ENABLED_STATUS(), etc.
//
// Only for use within fbfalcon applications.
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
// Also have FBFALCONCFG_CHIP_ENABLED(), FBFALCONCFG_FEATURE_ENABLED(), etc
// from {{$XXCONFIG_H_FILE}}.
//
// NOTE: these definitions are "flat" (ie they don't use some more general
//       FBFALCONCFG_ENABLED(CHIP,X) form because the pre-processor would re-evaluate
//       the expansion of the item (gpu, feature, class, api).  For classes,
//       at least, this is a problem since we would end up with class number
//       instead of its name...

// hack: MSVC is not C99 compliant
#ifdef LW_WINDOWS
#define __func__ __FUNCTION__
#endif

// CHIP's
#define FBFALCONCFG_CHIP_ENABLED_STATUS(W)   (FBFALCONCFG_CHIP_##W ? {{$STATUS_OK}} : {{$STATUS_ERROR}})
#define FBFALCONCFG_CHIP_ENABLED_OR_BAIL(W)                \
     do {                                           \
         if ( ! FBFALCONCFG_CHIP_##W)                      \
         {                                          \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "FBFALCON: %s: CHIP " #W " not enabled, bailing\n", __func__)); \
             return {{$STATUS_ERROR}};           \
         }                                          \
     } while (LW_FALSE)
#define FBFALCONCFG_CHIP_ENABLED_OR_ASSERT_AND_BAIL(W)     \
     do {                                           \
         if ( ! FBFALCONCFG_CHIP_##W)                      \
         {                                          \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "FBFALCON: %s: CHIP " #W " not enabled, assert and bail\n", __func__)); \
             {{$ASSERT_DEFINE}}(FBFALCONCFG_CHIP_##W);                        \
             return {{$STATUS_ERROR}};           \
         }                                          \
     } while (LW_FALSE)

// FEATURE's
#define FBFALCONCFG_FEATURE_ENABLED_STATUS(W)   (FBFALCONCFG_FEATURE_##W ? {{$STATUS_OK}} : {{$STATUS_ERROR}})
#define FBFALCONCFG_FEATURE_ENABLED_OR_BAIL(W)            \
     do {                                           \
         if ( ! FBFALCONCFG_FEATURE_##W)                  \
         {                                          \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "FBFALCON: %s: FEATURE " #W " not enabled, bailing\n", __func__)); \
             return {{$STATUS_ERROR}};           \
         }                                          \
     } while (LW_FALSE)
#define FBFALCONCFG_FEATURE_ENABLED_OR_ASSERT_AND_BAIL(W) \
     do {                                           \
         if ( ! FBFALCONCFG_FEATURE_##W)                  \
         {                                          \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "FBFALCON: %s: FEATURE " #W " not enabled, assert and bail\n", __func__)); \
             {{$ASSERT_DEFINE}}(FBFALCONCFG_FEATURE_##W);          \
             return {{$STATUS_ERROR}};           \
         }                                          \
     } while (LW_FALSE)

// ENGINE's
#define FBFALCONCFG_ENGINE_ENABLED_STATUS(W)   (FBFALCONCFG_ENGINE_##W ? {{$STATUS_OK}} : {{$STATUS_ERROR}})
#define FBFALCONCFG_ENGINE_ENABLED_OR_BAIL(W)             \
     do {                                           \
         if ( ! FBFALCONCFG_ENGINE_##W)                   \
         {                                          \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "FBFALCON: %s: ENGINE " #W " not enabled, bailing\n", __func__)); \
             return {{$STATUS_ERROR}};           \
         }                                          \
     } while (LW_FALSE)
#define FBFALCONCFG_ENGINE_ENABLED_OR_ASSERT_AND_BAIL(W)  \
     do {                                           \
         if ( ! FBFALCONCFG_ENGINE_##W)                   \
         {                                          \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "FBFALCON: %s: ENGINE " #W " not enabled, assert and bail\n", __func__)); \
             {{$ASSERT_DEFINE}}(FBFALCONCFG_ENGINE_##W);           \
             return {{$STATUS_ERROR}};           \
         }                                          \
     } while (LW_FALSE)

// CLASS's
#define FBFALCONCFG_CLASS_ENABLED_STATUS(W)   (FBFALCONCFG_CLASS_##W ? {{$STATUS_OK}} : {{$STATUS_ERROR}})
#define FBFALCONCFG_CLASS_ENABLED_OR_BAIL(W)              \
     do {                                           \
         if ( ! FBFALCONCFG_CLASS_##W)                    \
         {                                          \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "FBFALCON: %s: CLASS " #W " not enabled, bailing\n", __func__)); \
             return {{$STATUS_ERROR}};           \
         }                                          \
     } while (LW_FALSE)
#define FBFALCONCFG_CLASS_ENABLED_OR_ASSERT_AND_BAIL(W)   \
     do {                                           \
         if ( ! FBFALCONCFG_CLASS_##W)                    \
         {                                          \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "FBFALCON: %s: CLASS " #W " not enabled, assert and bail\n", __func__)); \
             {{$ASSERT_DEFINE}}(FBFALCONCFG_CLASS_##W);            \
             return {{$STATUS_ERROR}};           \
         }                                          \
     } while (LW_FALSE)

// API's
#define FBFALCONCFG_API_ENABLED_STATUS(W)   (FBFALCONCFG_API_##W ? {{$STATUS_OK}} : {{$STATUS_ERROR}})
#define FBFALCONCFG_API_ENABLED_OR_BAIL(W)                \
     do {                                           \
         if ( ! FBFALCONCFG_API_##W)                      \
         {                                          \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "FBFALCON: %s: API " #W " not enabled, bailing\n", __func__)); \
             return {{$STATUS_ERROR}};           \
         }                                          \
     } while (LW_FALSE)
#define FBFALCONCFG_API_ENABLED_OR_ASSERT_AND_BAIL(W)     \
     do {                                           \
         if ( ! FBFALCONCFG_API_##W)                      \
         {                                          \
             DBG_PRINTF((DBG_MODULE_GLOBAL, DBG_LEVEL_ERRORS, "FBFALCON: %s: API " #W " not enabled, assert and bail\n", __func__)); \
             {{$ASSERT_DEFINE}}(FBFALCONCFG_API_##W);              \
             return {{$STATUS_ERROR}};           \
         }                                          \
     } while (LW_FALSE)


#endif  // G_{{$XXCFG}}_PRIVATE_H
