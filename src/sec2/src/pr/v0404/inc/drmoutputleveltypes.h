/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

/****************************************************************************************************
**                                                                                                 **
** Most structures in this file are versioned for binary compatibility.                            **
**                                                                                                 **
** Your application code should always use the versioned structure names, e.g.                     **
** DRM_PLAY_OPL_3_0 and not DRM_PLAY_OPL_LATEST, so that it can be upgraded to future              **
** PK versions without callback implementation changes excluding any required by                   **
** any changes to the PlayReady compliance and robustness rules for that future version.           **
**                                                                                                 **
** If your application is only talking to the latest version of the PK,                            **
** only the *_LATEST structures/constants are relevant for versioned structures.                   **
**                                                                                                 **
** However, if your application is designed to talk to multiple versions of the PK,                **
** you may need to check the version on a structure and, if it's older than latest,                **
** cast it to the older structure corresponding to that version.                                   **
**                                                                                                 **
** For example, if the version of the structure you receive matches the _2_0 constant              **
** instead of the _LATEST constant, you need to cast that structure to the _2_0                    **
** structure instead of directly using the _3_0 structure you received.                            **
** This will be the case when your application talks to the PK 2.0 version of the PK               **
** but not when it talks to the PK 3.0 version of the PK.                                          **
**                                                                                                 **
** In addition, some versions of the PK did not properly set their structure versions.             **
** If the older PKs you are talking include any which indicate "Patch required" below,             **
** each older PK must be patched or you will receive incorrect structure versions.                 **
**                                                                                                 **
****************************************************************************************************/

/****************************************************************************************************
**                                                                                                 **
** WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING **
**                                                                                                 **
** You should ignore this warning if you are a developer who has licensed the PK from Microsoft.   **
**                                                                                                 **
** This warning is for Microsoft internal developers working on the porting kit itself.            **
**                                                                                                 **
** Structures in this file must NOT be modified at a binary/layout level.                          **
** Doing so will break PK customer upgrade scenarios.                                              **
**                                                                                                 **
** Instead, create new versioned definitions of any impacted structures, add new version #defines, **
** and update any impacted _LATEST typdef/#define lines to point to the updated versions.          **
**                                                                                                 **
** The only existing lines of code in this file that should CHANGE are the _LATEST lines.          **
** typedef ... DRM_PLAY_OPL_LATEST and #define VER_DRM_PLAY_OPL_LATEST ... should ALWAYS change.   **
** Other existing typedefs/#defines should only change if they are being modified from the         **
** previous version, i.e. newer structure definitions CAN/SHOULD depend on older structure         **
** definitions whenever possible using new typedefs/#defines (e.g. typedef foo_3_0 foo_4_5).       **
**                                                                                                 **
** Other than that, changes should result only in ADDED code for new structures/constants.         **
**                                                                                                 **
** PK source code (most code under source\...) should use the _LATEST structures.                  **
** Exception: The comment in drmcallbacktypes.h uses the current versioned structure.              **
**                                                                                                 **
** PK test/tools code (all code under test\..., code under source\tools\... and a few others)      **
** should always use the *versioned* structures just like an end customer will as described above. **
** This means that code should continue to compile/work as-is after changes to the source code!    **
** New test/tools code should be added as appropriate to act on any new data in the structures!    **
**                                                                                                 **
** Breaking changes are fine as long as versioning is done properly, but ALL changes must be       **
** versioned regardless of whether they are breaking or not - e.g. adding a new member at the      **
** end of a structure still requires updated structure versions.                                   **
**                                                                                                 **
** For example, compare DRM_PLAY_OPL_2_0 versus DRM_PLAY_OPL_3_0.                                  **
** Even if only the dvopi member was added, a new structure version would have been introduced.    **
**                                                                                                 **
** WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING **
**                                                                                                 **
****************************************************************************************************/

#ifndef __DRM_OUTPUTLEVELTYPES_H__
#define __DRM_OUTPUTLEVELTYPES_H__

#include <drmtypes.h>

ENTER_PK_NAMESPACE;

static DRM_GLOBAL_CONST DRM_WORD DRM_DEFAULT_MINIMUM_SELWRITY_LEVEL = 100;

#define DRM_MAX_EXPLICIT_OUTPUT_PROTECTION_CONFIG_DATA      32

typedef struct __tagDRM_MINIMUM_OUTPUT_PROTECTION_LEVELS
{
    DRM_WORD wCompressedDigitalVideo;
    DRM_WORD wUncompressedDigitalVideo;
    DRM_WORD wAnalogVideo;
    DRM_WORD wCompressedDigitalAudio;
    DRM_WORD wUncompressedDigitalAudio;
} DRM_MINIMUM_OUTPUT_PROTECTION_LEVELS;

/* Exists only to maintain binary compatibility of structures. */
typedef struct __tagDRM_OPL_OUTPUT_IDS_RESERVED
{
    DRM_WORD  cIds;
    DRM_GUID  *rgIds;
} DRM_OPL_OUTPUT_IDS_RESERVED;


/*
** Structures representing a single output protection entry.
** Structures changed in 3.0 QFE 3 to change dwConfigData to rgbConfigData/cbConfigData.
** 2.0:                     No patch required.  Versions are properly set to 2 or 3.
** 3.0 (with QFE 3) to 4.0: Patch required to properly set versions to 4 instead of 2 or 3.
** 4.1+:                    No patch required.  Versions are properly set to 4.
*/

/* 2.0 structures */
#define VER_DRM_ANALOG_VIDEO_OUTPUT_PROTECTION_2_0      2
#define VER_DRM_AUDIO_OUTPUT_PROTECTION_2_0             3
typedef struct __tagDRM_OUTPUT_PROTECTION_2_0
{
    DRM_DWORD   dwVersion;
    DRM_GUID    guidId;
    DRM_DWORD   dwConfigData;
} DRM_OUTPUT_PROTECTION_2_0;

typedef DRM_OUTPUT_PROTECTION_2_0 DRM_ANALOG_VIDEO_OUTPUT_PROTECTION_2_0;
typedef DRM_OUTPUT_PROTECTION_2_0 DRM_AUDIO_OUTPUT_PROTECTION_2_0;

/* 3.0 (with QFE 3) definitions */
#define VER_DRM_ANALOG_VIDEO_OUTPUT_PROTECTION_3_0      4
#define VER_DRM_AUDIO_OUTPUT_PROTECTION_3_0             4
#define VER_DRM_DIGITAL_VIDEO_OUTPUT_PROTECTION_3_0     4
typedef struct __tagDRM_OUTPUT_PROTECTION_3_0
{
    DRM_DWORD   dwVersion;
    DRM_GUID    guidId;
    DRM_BYTE    rgbConfigData[ DRM_MAX_EXPLICIT_OUTPUT_PROTECTION_CONFIG_DATA ];
    DRM_DWORD   cbConfigData;
} DRM_OUTPUT_PROTECTION_3_0;

typedef DRM_OUTPUT_PROTECTION_3_0 DRM_ANALOG_VIDEO_OUTPUT_PROTECTION_3_0;
typedef DRM_OUTPUT_PROTECTION_3_0 DRM_AUDIO_OUTPUT_PROTECTION_3_0;
typedef DRM_OUTPUT_PROTECTION_3_0 DRM_DIGITAL_VIDEO_OUTPUT_PROTECTION_3_0;

/* latest definitions */
#define VER_DRM_ANALOG_VIDEO_OUTPUT_PROTECTION_LATEST   VER_DRM_ANALOG_VIDEO_OUTPUT_PROTECTION_3_0
#define VER_DRM_AUDIO_OUTPUT_PROTECTION_LATEST          VER_DRM_AUDIO_OUTPUT_PROTECTION_3_0
#define VER_DRM_DIGITAL_VIDEO_OUTPUT_PROTECTION_LATEST  VER_DRM_DIGITAL_VIDEO_OUTPUT_PROTECTION_3_0
typedef DRM_ANALOG_VIDEO_OUTPUT_PROTECTION_3_0          DRM_ANALOG_VIDEO_OUTPUT_PROTECTION_LATEST;
typedef DRM_AUDIO_OUTPUT_PROTECTION_3_0                 DRM_AUDIO_OUTPUT_PROTECTION_LATEST;
typedef DRM_DIGITAL_VIDEO_OUTPUT_PROTECTION_3_0         DRM_DIGITAL_VIDEO_OUTPUT_PROTECTION_LATEST;
typedef DRM_OUTPUT_PROTECTION_3_0                       DRM_OUTPUT_PROTECTION_LATEST;



/*
** Structures each representing a list of output protection entries.
** Structures changed in 3.0 QFE 3 due to change in child structure DRM_OUTPUT_PROTECTION_*.
** 2.x:                     No patch required.  Versions are properly set to 0.
** 3.0 (with QFE 3) to 4.0: Patch required to properly set versions to 1 instead of 0.
** 4.1+:                    No patch required.  Versions are properly set to 1.
*/

/* 2.0 definitions */
#define VER_DRM_ANALOG_VIDEO_OUTPUT_PROTECTION_IDS_2_0      0
#define VER_DRM_AUDIO_OUTPUT_PROTECTION_IDS_2_0             0
typedef struct __tagDRM_ANALOG_VIDEO_OUTPUT_PROTECTION_IDS_2_0
{
    DRM_DWORD   dwVersion;
    DRM_WORD    cEntries;
    DRM_ANALOG_VIDEO_OUTPUT_PROTECTION_2_0 *rgVop;
} DRM_ANALOG_VIDEO_OUTPUT_PROTECTION_IDS_2_0;

typedef struct __tagDRM_AUDIO_OUTPUT_PROTECTION_IDS_2_0
{
    DRM_DWORD   dwVersion;
    DRM_WORD    cEntries;
    DRM_AUDIO_OUTPUT_PROTECTION_2_0 *rgAop;
} DRM_AUDIO_OUTPUT_PROTECTION_IDS_2_0;

/* 3.0 (with QFE 3) definitions */
#define VER_DRM_ANALOG_VIDEO_OUTPUT_PROTECTION_IDS_3_0      1
#define VER_DRM_AUDIO_OUTPUT_PROTECTION_IDS_3_0             1
#define VER_DRM_DIGITAL_VIDEO_OUTPUT_PROTECTION_IDS_3_0     1
typedef struct __tagDRM_ANALOG_VIDEO_OUTPUT_PROTECTION_IDS_3_0
{
    DRM_DWORD   dwVersion;
    DRM_WORD    cEntries;
    DRM_ANALOG_VIDEO_OUTPUT_PROTECTION_3_0 *rgVop;
} DRM_ANALOG_VIDEO_OUTPUT_PROTECTION_IDS_3_0;

typedef struct __tagDRM_AUDIO_OUTPUT_PROTECTION_IDS_3_0
{
    DRM_DWORD   dwVersion;
    DRM_WORD    cEntries;
    DRM_AUDIO_OUTPUT_PROTECTION_3_0 *rgAop;
} DRM_AUDIO_OUTPUT_PROTECTION_IDS_3_0;

typedef struct __tagDRM_DIGITAL_VIDEO_OUTPUT_PROTECTION_IDS_3_0
{
    DRM_DWORD   dwVersion;
    DRM_WORD    cEntries;
    DRM_DIGITAL_VIDEO_OUTPUT_PROTECTION_3_0 *rgVop;
} DRM_DIGITAL_VIDEO_OUTPUT_PROTECTION_IDS_3_0;

/* latest definitions */
#define VER_DRM_ANALOG_VIDEO_OUTPUT_PROTECTION_IDS_LATEST   VER_DRM_ANALOG_VIDEO_OUTPUT_PROTECTION_IDS_3_0
#define VER_DRM_AUDIO_OUTPUT_PROTECTION_IDS_LATEST          VER_DRM_AUDIO_OUTPUT_PROTECTION_IDS_3_0
#define VER_DRM_DIGITAL_VIDEO_OUTPUT_PROTECTION_IDS_LATEST  VER_DRM_DIGITAL_VIDEO_OUTPUT_PROTECTION_IDS_3_0
typedef DRM_ANALOG_VIDEO_OUTPUT_PROTECTION_IDS_3_0  DRM_ANALOG_VIDEO_OUTPUT_PROTECTION_IDS_LATEST;
typedef DRM_AUDIO_OUTPUT_PROTECTION_IDS_3_0         DRM_AUDIO_OUTPUT_PROTECTION_IDS_LATEST;
typedef DRM_DIGITAL_VIDEO_OUTPUT_PROTECTION_IDS_3_0 DRM_DIGITAL_VIDEO_OUTPUT_PROTECTION_IDS_LATEST;



/*
** Structure representing all output protection information.
** Structure changed in 3.0 QFE 3 to add dvopi member.
** Structure changed in 3.0 QFE 3 due to change in child structures DRM_*_OUTPUT_PROTECTION_IDS_*.
** 2.x:                     No patch required.  Version is properly set to 0.
** 3.0 (with QFE 3) to 4.0: Patch required to properly set versions to 1 instead of 0.
** 4.1+:        No patch required.  Version is properly set to 1.
*/

/* 2.0 definitions */
#define VER_DRM_PLAY_OPL_2_0    0
typedef struct __tagDRM_PLAY_OPL_2_0
{
    DRM_DWORD                                   dwVersion;
    DRM_MINIMUM_OUTPUT_PROTECTION_LEVELS        minOPL;
    DRM_OPL_OUTPUT_IDS_RESERVED                 oplIdReserved;
    DRM_ANALOG_VIDEO_OUTPUT_PROTECTION_IDS_2_0  vopi;
    DRM_AUDIO_OUTPUT_PROTECTION_IDS_2_0         aopi;
} DRM_PLAY_OPL_2_0;

/* 3.0 (with QFE 3) definitions */
#define VER_DRM_PLAY_OPL_3_0    2
typedef struct __tagDRM_PLAY_OPL_3_0
{
    DRM_DWORD                                   dwVersion;
    DRM_MINIMUM_OUTPUT_PROTECTION_LEVELS        minOPL;
    DRM_OPL_OUTPUT_IDS_RESERVED                 oplIdReserved;
    DRM_ANALOG_VIDEO_OUTPUT_PROTECTION_IDS_3_0  vopi;
    DRM_AUDIO_OUTPUT_PROTECTION_IDS_3_0         aopi;
    DRM_DIGITAL_VIDEO_OUTPUT_PROTECTION_IDS_3_0 dvopi;
} DRM_PLAY_OPL_3_0;

/* latest definitions */
#define VER_DRM_PLAY_OPL_LATEST VER_DRM_PLAY_OPL_3_0
typedef DRM_PLAY_OPL_3_0 DRM_PLAY_OPL_LATEST;


/* typedefs which enable client code using legacy struct names to continue to compile */
typedef DRM_OPL_OUTPUT_IDS_RESERVED                 DRM_OPL_OUTPUT_IDS;
typedef DRM_OUTPUT_PROTECTION_2_0                   DRM_OUTPUT_PROTECTION_EX;
typedef DRM_ANALOG_VIDEO_OUTPUT_PROTECTION_2_0      DRM_VIDEO_OUTPUT_PROTECTION_EX;
typedef DRM_AUDIO_OUTPUT_PROTECTION_2_0             DRM_AUDIO_OUTPUT_PROTECTION_EX;
typedef DRM_ANALOG_VIDEO_OUTPUT_PROTECTION_IDS_2_0  DRM_VIDEO_OUTPUT_PROTECTION_IDS_EX;
typedef DRM_AUDIO_OUTPUT_PROTECTION_IDS_2_0         DRM_AUDIO_OUTPUT_PROTECTION_IDS_EX;
typedef DRM_PLAY_OPL_2_0                            DRM_PLAY_OPL_EX2;

EXIT_PK_NAMESPACE;

#endif /* __DRM_OUTPUTLEVELTYPES_H__ */

