#************************ BEGIN COPYRIGHT NOTICE ***************************#
#                                                                           #
#          Copyright (c) LWPU Corporation.  All rights reserved.          #
#                                                                           #
# All information contained herein is proprietary and confidential to       #
# LWPU Corporation.  Any use, reproduction, or disclosure without the     #
# written permission of LWPU Corporation is prohibited.                   #
#                                                                           #
#************************** END COPYRIGHT NOTICE ***************************#
#
# Defines all known SOE DMEM overlays.
#
# Overlays:
#     An "overlay" is any generic piece of data that can be paged in and out of
#     DMEM at runtime. To gcc, this is simply a data section. That is, the
#     overlays used here are not considered by gcc as true overlays as each
#     section has its own unique data address-space.
#
#     NOTE: The ordering of the overlay definitions in this module is
#           significant. Overlays will be placed in the final image in the
#           order that they are defined in this file.
#
# Overlay Attributes:
#
#     DESCRIPTION
#         Optional description of the DMEM overlay.
#
#     NAME
#         Defines the name of the output-section that is created for the
#         overlay. By default, the overlay name is comes from the hash key used
#         below when defining the overlay. The key name is colwerted to
#         camel-case with the leading character in lower-case. This attribute
#         may be used as an override if the resulting name is undesirable.
#
#     SECTION_DESCRIPTION
#         This attribute may be used to specify the exact linker-script text
#         used to define the overlay. It overrides all other attributes (such
#         as the INPUT_SECTIONS) attribute. It should only be used under
#         special cirlwmstances where special syntax is required that is not
#         supported by the linker-script generation tool (ldgen).
#
#     MAX_SIZE_BLOCKS
#         The maximum size of this DMEM overlay in blocks (256 byte units)
#
#     FLAGS
#         Specifies extra attributes using flags, as below:
#         :RESIDENT
#             Specifies that the overlay is a resident overlay (so will be
#             loaded into resident area). It is loaded by the bootloader when
#             booting RTOS, and is never a candidate to be kicked out of
#             physical memory. So we don't need to explicitly load/unload it
#             from the app level.
#
#     ENABLED_ON
#         Defines the list of chips on which the overlay is enabled. If an
#         overlay is not used on a particular chip, that chip must be excluded
#         from this list. We generate symbols in DMEM for every overlay that is
#         enabled, and that oclwpies (limited) DMEM resources. Hence, it is
#         important to make sure that we only enable the overlay only on chips
#         where they are used. Every overlay must specify this attribute.
#
#

my $dmemOverlaysRaw =
[

];

# return the raw data of Overlays definition
return $dmemOverlaysRaw;

