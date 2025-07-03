// This is a dummy VMIOP config file, so that mods can include
// vmioplugin.h in builds that don't compile the VMIOP plugin.
//
// If possible, mods should use the real VMIOP config file, which is
// generated when we compile the VMIOP plugin.  This dummy file should
// not be in the include path for builds that compile the plugin.

#define VMIOPLUGINCFG_FEATURE_ENABLED(x) 0
