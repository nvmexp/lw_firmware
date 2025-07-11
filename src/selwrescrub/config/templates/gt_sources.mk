# This file is automatically generated by {{$name}} - DO NOT EDIT!
#
# Source file lists.
#
# Profile:  {{$PROFILE}}
# Template: {{$TEMPLATE_FILE}}
#
# Sources list: {{ SOURCE_LIST(':Q_SOURCES_FILE') }}
#
# Platform:     {{ SOURCE_LIST(':Q_ENABLED_PLATFORMS') }}
#
# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Architecture/Resman_Adding_Files
#

# This will be list of all files
{{$ABBREV}}SRC =

# File lists organized by var, eg: {{$ABBREV}}SRC_CORE, {{$ABBREV}}SRC_GR, etc.
{{ SOURCE_LIST(':FILES_BY_VAR') }}

# File lists organized by platform sections, eg: {{$ABBREV}}CORE, RMHAL_TESLA, etc.
{{ SOURCE_LIST(':FILES_BY_SECTION') }}

