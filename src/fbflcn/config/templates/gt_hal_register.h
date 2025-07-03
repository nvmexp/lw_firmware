//
// Hal registration entry points for FBFALCON
//
// Profile:  {{$PROFILE}}
// Template: {{$TEMPLATE_FILE}}
//
// Chips:    {{ CHIP_LIST() }}
//

#ifndef G_{{$XXCFG}}_HAL_REGISTER_H
#define G_{{$XXCFG}}_HAL_REGISTER_H

//
// per-family HAL registration entry points
//

{{ HAL_REGISTER_CHIPS(":ENABLED:BY_CHIP_FAMILY") }}

#endif  // G_{{$XXCFG}}_HAL_REGISTER_H
