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
# Defines all known SEC2 IMEM overlays.
#
# Overlays:
#     An "overlay" is any generic piece of code that can be paged in and out of
#     IMEM at runtime. To gcc, this is simply a code section. That is, the
#     overlays used here are not considered by gcc as true overlays as each
#     section has its own unique code address-space.
#
#     NOTE: The ordering of the overlay definitions in this module is
#           significant. Overlays will be placed in the final image in the
#           order that they are defined in this file.
#
# Overlay Attributes:
#
#     DESCRIPTION
#         Optional description of the overlay.
#
#     NAME
#         Defines the name of the output-section that is created for the
#         overlay. By default, the overlay name is comes from the hash key used
#         below when defining the overlay. The key name is colwerted to
#         camel-case with the leading character in lower-case. This attribute
#         may be used as an override if the resulting name is undesirable.
#
#     INPUT_SECTIONS
#         Defines a two-dimensional array where each top-level array element
#         defines a filename,section pair. The filename may refer to a specific
#         object file or a file glob. Combined with the section, the pair
#         represents an input code-section that will be mapped into the overlay
#         (output-section) by the linker.
#
#     KEEP_SECTIONS
#         Defines a one-dimensional array where each array element defines a
#         section name that we want the linker to not use link-time garbage
#         collection. It is useful to mark sections that should not be
#         eliminated. Today, we use these for HS overlays whose entry functions
#         are not explicitly called and are, thus, automatic candidates for
#         garbage collection. However, they are called by creating a function
#         pointer at the top of the overlay block. Hence we need them to be
#         present in the final image and also to be placed at the top of the
#         overlay in order to validate the overlay before jumping in. Hence,
#         all keep sections will be put at the top (lower addresses) of the
#         overlay. However, this is based on our use case today. It isn't
#         essential to do. The input sections will follow these at higher
#         addresses. This attribute may be used even in non-HS overlays, but
#         setting it for HS overlays is enforced.
#
#     SECTION_DESCRIPTION
#         This attribute may be used to specify the exact linker-script text
#         used to define the overlay. It overrides all other attributes (such
#         as the INPUT_SECTIONS) attribute. It should only be used under
#         special cirlwmstances where special syntax is required that is not
#         supported by the linker-script generation tool (ldgen).
#
#     ALIGNMENT
#         Defines the starting alignment of the overlay. By default, overlays
#         are not aligned. Each overlay starts where the previous overlay
#         ended. This attribute may be used if a specific alignment is required
#         (e.g. 256-byte alignment). If an overlay is an HS_OVERLAY, even if
#         this attribute is unspecified or an alignment different from 256-byte
#         alignment is specified, we will enforce 256-byte alignment.
#
#    FLAGS
#         Specifies extra attributes using flags, as below:
#         :HS_OVERLAY
#             This attribute may be used to indicate that the overlay is a HS
#             overlay and needs to be placed at the top (lower addresses) of
#             the virtual address space of the image. When HS_OVERLAY is set,
#             we require that KEEP_SECTIONS is set whereby the entry function
#             of the overlay may not be used as a candidate for link-time
#             garbage collection. We also internally enforce a 256-byte
#             alignment for HS overlays, irrespective of what value is set in
#             the ALIGNMENT attribute, or whether it is set at all. If this
#             attribute is not set, it will be compiled as a non-secure (NS) or
#             a light secure (LS) overlay, depending on the profile.
#
#         :HS_LIB
#             Specifies that the overlay is a HS library overlay, that is,
#             a HS overlay that may be shared between other HS overlays. HS
#             library overlays are placed at the top (lower addresses) of the
#             virtual address space, above other HS overlays. This attribute
#             takes effect only when :HS_OVERLAY is set.
#
#         :RESIDENT
#             Specifies that the overlay is a resident overlay, and is never a
#             candidate to be kicked out of physical memory. It will be loaded
#             by the SEC2 application at the very beginning after being booted.
#             There is really no need for this flag to be used with non-heavy
#             secure code since we might as well place it in the contiguous
#             resident code section. With heavy secure code, overlay placement
#             in virtual address space is important because the signature
#             depends on the start address of the overlay. Hence, not only do
#             we need to place heavy secure code at the lower addresses of the
#             virtual address space to avoid needing to resign, but sometimes,
#             we also need to strategically decide positions among HS overlays
#             such that the ones already signed and least likely to change will
#             be at the lowest addresses, thus allowing start address of code
#             after it to change as little as possible. Hence, we cannot really
#             make heavy secure code contiguous with the resident code in case
#             some of it wants to be resident, and in those cases, we use this
#             flag to indicate which overlays intend to be resident.
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

#
# FBFalcon lwrrently does not deploy overlays, this section is required for link script
# populaton but is not populated.

my $imemOverlaysRaw =
[
];

# return the raw data of Overlays definition
return $imemOverlaysRaw;

