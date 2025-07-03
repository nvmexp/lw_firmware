/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <drmconstants.h>
#include <drmlicense.h>
#include <drmbytemanip.h>


ENTER_PK_NAMESPACE_CODE;

#ifdef NONE
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_LID g_rgbSecStoreGlobalName =
{
    'g', 'l', 'o', 'b', 'a', 'l',
    '.', 's', 'e', 'c', 's', 't',
    'a', 't', 'e', '\0'
};

/* Character constants */
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchNull                                   = DRM_WCHAR_CAST('\0');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchMinus                                  = DRM_WCHAR_CAST('-');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchPlus                                   = DRM_WCHAR_CAST('+');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchForwardSlash                           = DRM_WCHAR_CAST('/');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchColon                                  = DRM_WCHAR_CAST(':');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchComma                                  = DRM_WCHAR_CAST(',');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchQuote                                  = DRM_WCHAR_CAST('\"');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchSingleQuote                            = DRM_WCHAR_CAST('\'');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchNewLine                                = DRM_WCHAR_CAST('\n');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchBackSlash                              = DRM_WCHAR_CAST('\\');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wch0                                      = DRM_WCHAR_CAST('0');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wch1                                      = DRM_WCHAR_CAST('1');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wch2                                      = DRM_WCHAR_CAST('2');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wch9                                      = DRM_WCHAR_CAST('9');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wcha                                      = DRM_WCHAR_CAST('a');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchd                                      = DRM_WCHAR_CAST('d');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchf                                      = DRM_WCHAR_CAST('f');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchh                                      = DRM_WCHAR_CAST('h');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchm                                      = DRM_WCHAR_CAST('m');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchn                                      = DRM_WCHAR_CAST('n');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchs                                      = DRM_WCHAR_CAST('s');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchx                                      = DRM_WCHAR_CAST('x');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchy                                      = DRM_WCHAR_CAST('y');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchz                                      = DRM_WCHAR_CAST('z');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchA                                      = DRM_WCHAR_CAST('A');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchF                                      = DRM_WCHAR_CAST('F');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchG                                      = DRM_WCHAR_CAST('G');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchM                                      = DRM_WCHAR_CAST('M');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchT                                      = DRM_WCHAR_CAST('T');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchX                                      = DRM_WCHAR_CAST('X');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchZ                                      = DRM_WCHAR_CAST('Z');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchUnderscore                             = DRM_WCHAR_CAST('_');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchPeriod                                 = DRM_WCHAR_CAST('.');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchQuestionMark                           = DRM_WCHAR_CAST('?');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchExclamationMark                        = DRM_WCHAR_CAST('!');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchOpenParen                              = DRM_WCHAR_CAST('(');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchCloseParen                             = DRM_WCHAR_CAST(')');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchPound                                  = DRM_WCHAR_CAST('#');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchSpace                                  = DRM_WCHAR_CAST(' ');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchTab                                    = DRM_WCHAR_CAST('\x9');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchLineFeed                               = DRM_WCHAR_CAST('\xA');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchVerticalTab                            = DRM_WCHAR_CAST('\xB');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchFormFeed                               = DRM_WCHAR_CAST('\xC');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchCarriageReturn                         = DRM_WCHAR_CAST('\xD');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchEqual                                  = DRM_WCHAR_CAST('=');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchOpenLwrly                              = DRM_WCHAR_CAST('{');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchCloseLwrly                             = DRM_WCHAR_CAST('}');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchLessThan                               = DRM_WCHAR_CAST('<');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchGreaterThan                            = DRM_WCHAR_CAST('>');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchLeftBracket                            = DRM_WCHAR_CAST('[');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchRightBracket                           = DRM_WCHAR_CAST(']');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchAsterisk                               = DRM_WCHAR_CAST('*');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchPercent                                = DRM_WCHAR_CAST('%');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchSemiColon                              = DRM_WCHAR_CAST(';');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchAmpersand                              = DRM_WCHAR_CAST('&');
DRM_GLOBAL_CONST                 DRM_WCHAR             g_wchPipe                                   = DRM_WCHAR_CAST('|');

/* Character constants - ANSI */
DRM_GLOBAL_CONST                 DRM_CHAR             g_chForwardSlash                             = '/';
DRM_GLOBAL_CONST                 DRM_CHAR             g_chPeriod                                   = '.';
DRM_GLOBAL_CONST                 DRM_CHAR             g_chNull                                     = '\0';
DRM_GLOBAL_CONST                 DRM_CHAR             g_chMinus                                    = '-';
DRM_GLOBAL_CONST                 DRM_CHAR             g_chPlus                                     = '+';
DRM_GLOBAL_CONST                 DRM_CHAR             g_ch0                                        = '0';
DRM_GLOBAL_CONST                 DRM_CHAR             g_ch9                                        = '9';
DRM_GLOBAL_CONST                 DRM_CHAR             g_cha                                        = 'a';
DRM_GLOBAL_CONST                 DRM_CHAR             g_chA                                        = 'A';

/* Misc strings shared across disparate functional areas */
DRM_STR_CONST        DRM_CHAR       g_rgchLicenseRespTag                         [] =  { DRM_INIT_CHAR_OBFUS('L'), DRM_INIT_CHAR_OBFUS('I'), DRM_INIT_CHAR_OBFUS('C'), DRM_INIT_CHAR_OBFUS('E'), DRM_INIT_CHAR_OBFUS('N'), DRM_INIT_CHAR_OBFUS('S'), DRM_INIT_CHAR_OBFUS('E'), DRM_INIT_CHAR_OBFUS('R'), DRM_INIT_CHAR_OBFUS('E'), DRM_INIT_CHAR_OBFUS('S'), DRM_INIT_CHAR_OBFUS('P'), DRM_INIT_CHAR_OBFUS('O'), DRM_INIT_CHAR_OBFUS('N'), DRM_INIT_CHAR_OBFUS('S'), DRM_INIT_CHAR_OBFUS('E'), DRM_INIT_CHAR_OBFUS('\0')}; /* ODD count */
DRM_STR_CONST        DRM_WCHAR      g_rgwchGUID                                  [] =  { DRM_INIT_WCHAR_OBFUS('G'), DRM_INIT_WCHAR_OBFUS('U'), DRM_INIT_WCHAR_OBFUS('I'), DRM_INIT_WCHAR_OBFUS('D'), DRM_INIT_WCHAR_OBFUS('\0') };
DRM_STR_CONST        DRM_WCHAR      g_rgwchFlag                                  [] =  { DRM_INIT_WCHAR_OBFUS('F'),  DRM_INIT_WCHAR_OBFUS('L'),  DRM_INIT_WCHAR_OBFUS('A'),  DRM_INIT_WCHAR_OBFUS('G'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchSecClockNotSet                        [] =  { DRM_INIT_WCHAR_OBFUS('D'),  DRM_INIT_WCHAR_OBFUS('R'),  DRM_INIT_WCHAR_OBFUS('M'),  DRM_INIT_WCHAR_OBFUS('_'),  DRM_INIT_WCHAR_OBFUS('C'),  DRM_INIT_WCHAR_OBFUS('L'),  DRM_INIT_WCHAR_OBFUS('K'),  DRM_INIT_WCHAR_OBFUS('_'),  DRM_INIT_WCHAR_OBFUS('N'),  DRM_INIT_WCHAR_OBFUS('O'),  DRM_INIT_WCHAR_OBFUS('T'),  DRM_INIT_WCHAR_OBFUS('_'),  DRM_INIT_WCHAR_OBFUS('S'),  DRM_INIT_WCHAR_OBFUS('E'),  DRM_INIT_WCHAR_OBFUS('T'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchSecClockSet                           [] =  { DRM_INIT_WCHAR_OBFUS('D'),  DRM_INIT_WCHAR_OBFUS('R'),  DRM_INIT_WCHAR_OBFUS('M'),  DRM_INIT_WCHAR_OBFUS('_'),  DRM_INIT_WCHAR_OBFUS('C'),  DRM_INIT_WCHAR_OBFUS('L'),  DRM_INIT_WCHAR_OBFUS('K'),  DRM_INIT_WCHAR_OBFUS('_'),  DRM_INIT_WCHAR_OBFUS('S'),  DRM_INIT_WCHAR_OBFUS('E'),  DRM_INIT_WCHAR_OBFUS('T'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchSecClockNeedsRefresh                  [] =  { DRM_INIT_WCHAR_OBFUS('D'),  DRM_INIT_WCHAR_OBFUS('R'),  DRM_INIT_WCHAR_OBFUS('M'),  DRM_INIT_WCHAR_OBFUS('_'),  DRM_INIT_WCHAR_OBFUS('C'),  DRM_INIT_WCHAR_OBFUS('L'),  DRM_INIT_WCHAR_OBFUS('K'),  DRM_INIT_WCHAR_OBFUS('_'),  DRM_INIT_WCHAR_OBFUS('N'),  DRM_INIT_WCHAR_OBFUS('E'),  DRM_INIT_WCHAR_OBFUS('E'),  DRM_INIT_WCHAR_OBFUS('D'),  DRM_INIT_WCHAR_OBFUS('S'),  DRM_INIT_WCHAR_OBFUS('_'),  DRM_INIT_WCHAR_OBFUS('R'),  DRM_INIT_WCHAR_OBFUS('E'),  DRM_INIT_WCHAR_OBFUS('F'),  DRM_INIT_WCHAR_OBFUS('R'),  DRM_INIT_WCHAR_OBFUS('E'),  DRM_INIT_WCHAR_OBFUS('S'),  DRM_INIT_WCHAR_OBFUS('H'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchExprVarSavedDateTime                  [] =  { DRM_INIT_WCHAR_OBFUS('s'),  DRM_INIT_WCHAR_OBFUS('a'),  DRM_INIT_WCHAR_OBFUS('v'),  DRM_INIT_WCHAR_OBFUS('e'),  DRM_INIT_WCHAR_OBFUS('d'),  DRM_INIT_WCHAR_OBFUS('d'),  DRM_INIT_WCHAR_OBFUS('a'),  DRM_INIT_WCHAR_OBFUS('t'),  DRM_INIT_WCHAR_OBFUS('e'),  DRM_INIT_WCHAR_OBFUS('t'),  DRM_INIT_WCHAR_OBFUS('i'),  DRM_INIT_WCHAR_OBFUS('m'),  DRM_INIT_WCHAR_OBFUS('e'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagMetering                           [] =  { DRM_INIT_WCHAR_OBFUS('M'),  DRM_INIT_WCHAR_OBFUS('E'),  DRM_INIT_WCHAR_OBFUS('T'),  DRM_INIT_WCHAR_OBFUS('E'),  DRM_INIT_WCHAR_OBFUS('R'),  DRM_INIT_WCHAR_OBFUS('D'),  DRM_INIT_WCHAR_OBFUS('A'),  DRM_INIT_WCHAR_OBFUS('T'),  DRM_INIT_WCHAR_OBFUS('A'),  DRM_INIT_WCHAR_OBFUS('\0')};

/* Rights */
DRM_STR_CONST        DRM_WCHAR      g_rgwchWMDRM_RIGHT_NONE                      [] =  { DRM_INIT_WCHAR_OBFUS('N'),  DRM_INIT_WCHAR_OBFUS('o'),  DRM_INIT_WCHAR_OBFUS('n'),  DRM_INIT_WCHAR_OBFUS('e'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchWMDRM_RIGHT_PLAYBACK                  [] =  { DRM_INIT_WCHAR_OBFUS('P'),  DRM_INIT_WCHAR_OBFUS('l'),  DRM_INIT_WCHAR_OBFUS('a'),  DRM_INIT_WCHAR_OBFUS('y'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchWMDRM_RIGHT_COLLABORATIVE_PLAY        [] =  { DRM_INIT_WCHAR_OBFUS('C'),  DRM_INIT_WCHAR_OBFUS('o'),  DRM_INIT_WCHAR_OBFUS('l'),  DRM_INIT_WCHAR_OBFUS('l'),  DRM_INIT_WCHAR_OBFUS('a'),  DRM_INIT_WCHAR_OBFUS('b'),  DRM_INIT_WCHAR_OBFUS('o'),  DRM_INIT_WCHAR_OBFUS('r'),  DRM_INIT_WCHAR_OBFUS('a'),  DRM_INIT_WCHAR_OBFUS('t'),  DRM_INIT_WCHAR_OBFUS('i'),  DRM_INIT_WCHAR_OBFUS('v'),  DRM_INIT_WCHAR_OBFUS('e'),  DRM_INIT_WCHAR_OBFUS('P'),  DRM_INIT_WCHAR_OBFUS('l'),  DRM_INIT_WCHAR_OBFUS('a'),  DRM_INIT_WCHAR_OBFUS('y'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchWMDRM_RIGHT_COPY_TO_CD                [] =  { DRM_INIT_WCHAR_OBFUS('P'),  DRM_INIT_WCHAR_OBFUS('r'),  DRM_INIT_WCHAR_OBFUS('i'),  DRM_INIT_WCHAR_OBFUS('n'),  DRM_INIT_WCHAR_OBFUS('t'),  DRM_INIT_WCHAR_OBFUS('.'),  DRM_INIT_WCHAR_OBFUS('r'),  DRM_INIT_WCHAR_OBFUS('e'),  DRM_INIT_WCHAR_OBFUS('d'),  DRM_INIT_WCHAR_OBFUS('b'),  DRM_INIT_WCHAR_OBFUS('o'),  DRM_INIT_WCHAR_OBFUS('o'),  DRM_INIT_WCHAR_OBFUS('k'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchWMDRM_RIGHT_COPY                      [] =  { DRM_INIT_WCHAR_OBFUS('C'),  DRM_INIT_WCHAR_OBFUS('o'),  DRM_INIT_WCHAR_OBFUS('p'),  DRM_INIT_WCHAR_OBFUS('y'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchWMDRM_RIGHT_CREATE_THUMBNAIL_IMAGE    [] =  { DRM_INIT_WCHAR_OBFUS('C'),  DRM_INIT_WCHAR_OBFUS('r'),  DRM_INIT_WCHAR_OBFUS('e'),  DRM_INIT_WCHAR_OBFUS('a'),  DRM_INIT_WCHAR_OBFUS('t'),  DRM_INIT_WCHAR_OBFUS('e'),  DRM_INIT_WCHAR_OBFUS('T'),  DRM_INIT_WCHAR_OBFUS('h'),  DRM_INIT_WCHAR_OBFUS('u'),  DRM_INIT_WCHAR_OBFUS('m'),  DRM_INIT_WCHAR_OBFUS('b'),  DRM_INIT_WCHAR_OBFUS('n'),  DRM_INIT_WCHAR_OBFUS('a'),  DRM_INIT_WCHAR_OBFUS('i'),  DRM_INIT_WCHAR_OBFUS('l'),  DRM_INIT_WCHAR_OBFUS('I'),  DRM_INIT_WCHAR_OBFUS('m'),  DRM_INIT_WCHAR_OBFUS('a'),  DRM_INIT_WCHAR_OBFUS('g'),  DRM_INIT_WCHAR_OBFUS('e'),  DRM_INIT_WCHAR_OBFUS('\0') };  /* L"CreateThumbnailImage"; */
DRM_STR_CONST        DRM_WCHAR      g_rgwchWMDRM_RIGHT_MOVE                      [] =  { DRM_INIT_WCHAR_OBFUS('M'),  DRM_INIT_WCHAR_OBFUS('o'),  DRM_INIT_WCHAR_OBFUS('v'),  DRM_INIT_WCHAR_OBFUS('e'),  DRM_INIT_WCHAR_OBFUS('\0')};

/* Script varibles used for license properties. */
DRM_STR_CONST        DRM_WCHAR      g_rgwchDRM_LS_COUNT_ATTR                     [] =  { DRM_INIT_WCHAR_OBFUS('C'),  DRM_INIT_WCHAR_OBFUS('o'),  DRM_INIT_WCHAR_OBFUS('u'),  DRM_INIT_WCHAR_OBFUS('n'),  DRM_INIT_WCHAR_OBFUS('t'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchDRM_LS_FIRSTUSE_ATTR                  [] =  { DRM_INIT_WCHAR_OBFUS('F'),  DRM_INIT_WCHAR_OBFUS('i'),  DRM_INIT_WCHAR_OBFUS('r'),  DRM_INIT_WCHAR_OBFUS('s'),  DRM_INIT_WCHAR_OBFUS('t'),  DRM_INIT_WCHAR_OBFUS('U'),  DRM_INIT_WCHAR_OBFUS('s'),  DRM_INIT_WCHAR_OBFUS('e'),  DRM_INIT_WCHAR_OBFUS('D'),  DRM_INIT_WCHAR_OBFUS('a'),  DRM_INIT_WCHAR_OBFUS('t'),  DRM_INIT_WCHAR_OBFUS('e'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchDRM_LS_FIRSTSTORE_ATTR                [] =  { DRM_INIT_WCHAR_OBFUS('F'),  DRM_INIT_WCHAR_OBFUS('i'),  DRM_INIT_WCHAR_OBFUS('r'),  DRM_INIT_WCHAR_OBFUS('s'),  DRM_INIT_WCHAR_OBFUS('t'),  DRM_INIT_WCHAR_OBFUS('S'),  DRM_INIT_WCHAR_OBFUS('t'),  DRM_INIT_WCHAR_OBFUS('o'),  DRM_INIT_WCHAR_OBFUS('r'),  DRM_INIT_WCHAR_OBFUS('e'),  DRM_INIT_WCHAR_OBFUS('D'),  DRM_INIT_WCHAR_OBFUS('a'),  DRM_INIT_WCHAR_OBFUS('t'),  DRM_INIT_WCHAR_OBFUS('e'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchDRM_LS_PLAYCOUNT_ATTR                 [] =  { DRM_INIT_WCHAR_OBFUS('P'),  DRM_INIT_WCHAR_OBFUS('l'),  DRM_INIT_WCHAR_OBFUS('a'),  DRM_INIT_WCHAR_OBFUS('y'),  DRM_INIT_WCHAR_OBFUS('C'),  DRM_INIT_WCHAR_OBFUS('o'),  DRM_INIT_WCHAR_OBFUS('u'),  DRM_INIT_WCHAR_OBFUS('n'),  DRM_INIT_WCHAR_OBFUS('t'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchDRM_LS_COPYCOUNT_ATTR                 [] =  { DRM_INIT_WCHAR_OBFUS('C'),  DRM_INIT_WCHAR_OBFUS('o'),  DRM_INIT_WCHAR_OBFUS('p'),  DRM_INIT_WCHAR_OBFUS('y'),  DRM_INIT_WCHAR_OBFUS('C'),  DRM_INIT_WCHAR_OBFUS('o'),  DRM_INIT_WCHAR_OBFUS('u'),  DRM_INIT_WCHAR_OBFUS('n'),  DRM_INIT_WCHAR_OBFUS('t'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchDRM_LS_DELETED_ATTR                   [] =  { DRM_INIT_WCHAR_OBFUS('d'),  DRM_INIT_WCHAR_OBFUS('e'),  DRM_INIT_WCHAR_OBFUS('l'),  DRM_INIT_WCHAR_OBFUS('e'),  DRM_INIT_WCHAR_OBFUS('t'),  DRM_INIT_WCHAR_OBFUS('e'),  DRM_INIT_WCHAR_OBFUS('d'),  DRM_INIT_WCHAR_OBFUS('\0') };  /*L"deleted";*/

/* Shared XML tags */
DRM_STR_CONST        DRM_WCHAR      g_rgwchAttributeVersion                      [] =  { DRM_INIT_WCHAR_OBFUS('v'),  DRM_INIT_WCHAR_OBFUS('e'),  DRM_INIT_WCHAR_OBFUS('r'),  DRM_INIT_WCHAR_OBFUS('s'),  DRM_INIT_WCHAR_OBFUS('i'),  DRM_INIT_WCHAR_OBFUS('o'),  DRM_INIT_WCHAR_OBFUS('n'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagData                               [] =  { DRM_INIT_WCHAR_OBFUS('D'),  DRM_INIT_WCHAR_OBFUS('A'),  DRM_INIT_WCHAR_OBFUS('T'),  DRM_INIT_WCHAR_OBFUS('A'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagIndex                              [] =  { DRM_INIT_WCHAR_OBFUS('I'),  DRM_INIT_WCHAR_OBFUS('N'),  DRM_INIT_WCHAR_OBFUS('D'),  DRM_INIT_WCHAR_OBFUS('E'),  DRM_INIT_WCHAR_OBFUS('X'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagPubkey                             [] =  { DRM_INIT_WCHAR_OBFUS('P'),  DRM_INIT_WCHAR_OBFUS('U'),  DRM_INIT_WCHAR_OBFUS('B'),  DRM_INIT_WCHAR_OBFUS('K'),  DRM_INIT_WCHAR_OBFUS('E'),  DRM_INIT_WCHAR_OBFUS('Y'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagValue                              [] =  { DRM_INIT_WCHAR_OBFUS('V'),  DRM_INIT_WCHAR_OBFUS('A'),  DRM_INIT_WCHAR_OBFUS('L'),  DRM_INIT_WCHAR_OBFUS('U'),  DRM_INIT_WCHAR_OBFUS('E'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagSignature                          [] =  { DRM_INIT_WCHAR_OBFUS('S'),  DRM_INIT_WCHAR_OBFUS('I'),  DRM_INIT_WCHAR_OBFUS('G'),  DRM_INIT_WCHAR_OBFUS('N'),  DRM_INIT_WCHAR_OBFUS('A'),  DRM_INIT_WCHAR_OBFUS('T'),  DRM_INIT_WCHAR_OBFUS('U'),  DRM_INIT_WCHAR_OBFUS('R'),  DRM_INIT_WCHAR_OBFUS('E'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagHashAlg                            [] =  { DRM_INIT_WCHAR_OBFUS('H'),  DRM_INIT_WCHAR_OBFUS('A'),  DRM_INIT_WCHAR_OBFUS('S'),  DRM_INIT_WCHAR_OBFUS('H'),  DRM_INIT_WCHAR_OBFUS('A'),  DRM_INIT_WCHAR_OBFUS('L'),  DRM_INIT_WCHAR_OBFUS('G'),  DRM_INIT_WCHAR_OBFUS('O'),  DRM_INIT_WCHAR_OBFUS('R'),  DRM_INIT_WCHAR_OBFUS('I'),  DRM_INIT_WCHAR_OBFUS('T'),  DRM_INIT_WCHAR_OBFUS('H'),  DRM_INIT_WCHAR_OBFUS('M'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagSignAlg                            [] =  { DRM_INIT_WCHAR_OBFUS('S'),  DRM_INIT_WCHAR_OBFUS('I'),  DRM_INIT_WCHAR_OBFUS('G'),  DRM_INIT_WCHAR_OBFUS('N'),  DRM_INIT_WCHAR_OBFUS('A'),  DRM_INIT_WCHAR_OBFUS('L'),  DRM_INIT_WCHAR_OBFUS('G'),  DRM_INIT_WCHAR_OBFUS('O'),  DRM_INIT_WCHAR_OBFUS('R'),  DRM_INIT_WCHAR_OBFUS('I'),  DRM_INIT_WCHAR_OBFUS('T'),  DRM_INIT_WCHAR_OBFUS('H'),  DRM_INIT_WCHAR_OBFUS('M'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchSHA                                   [] =  { DRM_INIT_WCHAR_OBFUS('S'),  DRM_INIT_WCHAR_OBFUS('H'),  DRM_INIT_WCHAR_OBFUS('A'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchMSDRM                                 [] =  { DRM_INIT_WCHAR_OBFUS('M'),  DRM_INIT_WCHAR_OBFUS('S'),  DRM_INIT_WCHAR_OBFUS('D'),  DRM_INIT_WCHAR_OBFUS('R'),  DRM_INIT_WCHAR_OBFUS('M'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_CHAR       g_rgchAttributeType                          [] =  { DRM_INIT_CHAR_OBFUS('t'),   DRM_INIT_CHAR_OBFUS('y'),   DRM_INIT_CHAR_OBFUS('p'),   DRM_INIT_CHAR_OBFUS('e'),   DRM_INIT_CHAR_OBFUS('\0') };
DRM_STR_CONST        DRM_WCHAR      g_rgwchAttributeType                         [] =  { DRM_INIT_WCHAR_OBFUS('t'),  DRM_INIT_WCHAR_OBFUS('y'),  DRM_INIT_WCHAR_OBFUS('p'),  DRM_INIT_WCHAR_OBFUS('e'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagCertificate                        [] =  { DRM_INIT_WCHAR_OBFUS('C'),  DRM_INIT_WCHAR_OBFUS('E'),  DRM_INIT_WCHAR_OBFUS('R'),  DRM_INIT_WCHAR_OBFUS('T'),  DRM_INIT_WCHAR_OBFUS('I'),  DRM_INIT_WCHAR_OBFUS('F'),  DRM_INIT_WCHAR_OBFUS('I'),  DRM_INIT_WCHAR_OBFUS('C'),  DRM_INIT_WCHAR_OBFUS('A'),  DRM_INIT_WCHAR_OBFUS('T'),  DRM_INIT_WCHAR_OBFUS('E'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagWrmHeader                          [] =  { DRM_INIT_WCHAR_OBFUS('W'),  DRM_INIT_WCHAR_OBFUS('R'),  DRM_INIT_WCHAR_OBFUS('M'),  DRM_INIT_WCHAR_OBFUS('H'),  DRM_INIT_WCHAR_OBFUS('E'),  DRM_INIT_WCHAR_OBFUS('A'),  DRM_INIT_WCHAR_OBFUS('D'),  DRM_INIT_WCHAR_OBFUS('E'),  DRM_INIT_WCHAR_OBFUS('R'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchAttributeVersion2Value                [] =  { DRM_INIT_WCHAR_OBFUS('2'),  DRM_INIT_WCHAR_OBFUS('.'),  DRM_INIT_WCHAR_OBFUS('0'),  DRM_INIT_WCHAR_OBFUS('.'),  DRM_INIT_WCHAR_OBFUS('0'),  DRM_INIT_WCHAR_OBFUS('.'),  DRM_INIT_WCHAR_OBFUS('0'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchAttributeVersion4Value                [] =  { DRM_INIT_WCHAR_OBFUS('4'),  DRM_INIT_WCHAR_OBFUS('.'),  DRM_INIT_WCHAR_OBFUS('0'),  DRM_INIT_WCHAR_OBFUS('.'),  DRM_INIT_WCHAR_OBFUS('0'),  DRM_INIT_WCHAR_OBFUS('.'),  DRM_INIT_WCHAR_OBFUS('0'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchAttributeVersion4_1Value              [] =  { DRM_INIT_WCHAR_OBFUS('4'),  DRM_INIT_WCHAR_OBFUS('.'),  DRM_INIT_WCHAR_OBFUS('1'),  DRM_INIT_WCHAR_OBFUS('.'),  DRM_INIT_WCHAR_OBFUS('0'),  DRM_INIT_WCHAR_OBFUS('.'),  DRM_INIT_WCHAR_OBFUS('0'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchAttributeVersion4_2Value              [] =  { DRM_INIT_WCHAR_OBFUS('4'),  DRM_INIT_WCHAR_OBFUS('.'),  DRM_INIT_WCHAR_OBFUS('2'),  DRM_INIT_WCHAR_OBFUS('.'),  DRM_INIT_WCHAR_OBFUS('0'),  DRM_INIT_WCHAR_OBFUS('.'),  DRM_INIT_WCHAR_OBFUS('0'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagLAINFO                             [] =  { DRM_INIT_WCHAR_OBFUS('L'),  DRM_INIT_WCHAR_OBFUS('A'),  DRM_INIT_WCHAR_OBFUS('I'),  DRM_INIT_WCHAR_OBFUS('N'),  DRM_INIT_WCHAR_OBFUS('F'),  DRM_INIT_WCHAR_OBFUS('O'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagV4DATA                             [] =  { DRM_INIT_WCHAR_OBFUS('V'),  DRM_INIT_WCHAR_OBFUS('4'),  DRM_INIT_WCHAR_OBFUS('D'),  DRM_INIT_WCHAR_OBFUS('A'),  DRM_INIT_WCHAR_OBFUS('T'),  DRM_INIT_WCHAR_OBFUS('A'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagLAURL                              [] =  { DRM_INIT_WCHAR_OBFUS('L'),  DRM_INIT_WCHAR_OBFUS('A'),  DRM_INIT_WCHAR_OBFUS('_'),  DRM_INIT_WCHAR_OBFUS('U'),  DRM_INIT_WCHAR_OBFUS('R'),  DRM_INIT_WCHAR_OBFUS('L'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagLUIURL                             [] =  { DRM_INIT_WCHAR_OBFUS('L'),  DRM_INIT_WCHAR_OBFUS('U'),  DRM_INIT_WCHAR_OBFUS('I'),  DRM_INIT_WCHAR_OBFUS('_'),  DRM_INIT_WCHAR_OBFUS('U'),  DRM_INIT_WCHAR_OBFUS('R'),  DRM_INIT_WCHAR_OBFUS('L'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagDSID                               [] =  { DRM_INIT_WCHAR_OBFUS('D'),  DRM_INIT_WCHAR_OBFUS('S'),  DRM_INIT_WCHAR_OBFUS('_'),  DRM_INIT_WCHAR_OBFUS('I'),  DRM_INIT_WCHAR_OBFUS('D'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagDataPubKey                         [] =  { DRM_INIT_WCHAR_OBFUS('P'),  DRM_INIT_WCHAR_OBFUS('U'),  DRM_INIT_WCHAR_OBFUS('B'),  DRM_INIT_WCHAR_OBFUS('L'),  DRM_INIT_WCHAR_OBFUS('I'),  DRM_INIT_WCHAR_OBFUS('C'),  DRM_INIT_WCHAR_OBFUS('K'),  DRM_INIT_WCHAR_OBFUS('E'),  DRM_INIT_WCHAR_OBFUS('Y'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagKID                                [] =  { DRM_INIT_WCHAR_OBFUS('K'),  DRM_INIT_WCHAR_OBFUS('I'),  DRM_INIT_WCHAR_OBFUS('D'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagKIDS                               [] =  { DRM_INIT_WCHAR_OBFUS('K'),  DRM_INIT_WCHAR_OBFUS('I'),  DRM_INIT_WCHAR_OBFUS('D'),  DRM_INIT_WCHAR_OBFUS('S'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagChecksum                           [] =  { DRM_INIT_WCHAR_OBFUS('C'),  DRM_INIT_WCHAR_OBFUS('H'),  DRM_INIT_WCHAR_OBFUS('E'),  DRM_INIT_WCHAR_OBFUS('C'),  DRM_INIT_WCHAR_OBFUS('K'),  DRM_INIT_WCHAR_OBFUS('S'),  DRM_INIT_WCHAR_OBFUS('U'),  DRM_INIT_WCHAR_OBFUS('M'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagAlgID                              [] =  { DRM_INIT_WCHAR_OBFUS('A'),  DRM_INIT_WCHAR_OBFUS('L'),  DRM_INIT_WCHAR_OBFUS('G'),  DRM_INIT_WCHAR_OBFUS('I'),  DRM_INIT_WCHAR_OBFUS('D'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagUplink                             [] =  { DRM_INIT_WCHAR_OBFUS('U'),  DRM_INIT_WCHAR_OBFUS('P'),  DRM_INIT_WCHAR_OBFUS('L'),  DRM_INIT_WCHAR_OBFUS('I'),  DRM_INIT_WCHAR_OBFUS('N'),  DRM_INIT_WCHAR_OBFUS('K'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagDecryptorSetup                     [] =  { DRM_INIT_WCHAR_OBFUS('D'),  DRM_INIT_WCHAR_OBFUS('E'),  DRM_INIT_WCHAR_OBFUS('C'),  DRM_INIT_WCHAR_OBFUS('R'),  DRM_INIT_WCHAR_OBFUS('Y'),  DRM_INIT_WCHAR_OBFUS('P'),  DRM_INIT_WCHAR_OBFUS('T'),  DRM_INIT_WCHAR_OBFUS('O'),  DRM_INIT_WCHAR_OBFUS('R'),  DRM_INIT_WCHAR_OBFUS('S'),  DRM_INIT_WCHAR_OBFUS('E'),  DRM_INIT_WCHAR_OBFUS('T'),  DRM_INIT_WCHAR_OBFUS('U'),  DRM_INIT_WCHAR_OBFUS('P'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagProtectInfo                        [] =  { DRM_INIT_WCHAR_OBFUS( 'P'), DRM_INIT_WCHAR_OBFUS( 'R'), DRM_INIT_WCHAR_OBFUS( 'O'), DRM_INIT_WCHAR_OBFUS( 'T'), DRM_INIT_WCHAR_OBFUS( 'E'), DRM_INIT_WCHAR_OBFUS( 'C'), DRM_INIT_WCHAR_OBFUS( 'T'), DRM_INIT_WCHAR_OBFUS( 'I'), DRM_INIT_WCHAR_OBFUS( 'N'), DRM_INIT_WCHAR_OBFUS( 'F'), DRM_INIT_WCHAR_OBFUS( 'O'), DRM_INIT_WCHAR_OBFUS( '\0') };
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagKeyLen                             [] =  { DRM_INIT_WCHAR_OBFUS( 'K'), DRM_INIT_WCHAR_OBFUS( 'E'), DRM_INIT_WCHAR_OBFUS( 'Y'), DRM_INIT_WCHAR_OBFUS( 'L'), DRM_INIT_WCHAR_OBFUS( 'E'), DRM_INIT_WCHAR_OBFUS( 'N'), DRM_INIT_WCHAR_OBFUS( '\0') };
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagKeyLenNodeDataCocktail             [] =  { DRM_INIT_WCHAR_OBFUS( '7'), DRM_INIT_WCHAR_OBFUS( '\0') };
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagKeyLenNodeDataAESCTR               [] =  { DRM_INIT_WCHAR_OBFUS( '1'), DRM_INIT_WCHAR_OBFUS( '6'), DRM_INIT_WCHAR_OBFUS( '\0') };
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagCOCKTAIL                           [] =  { DRM_INIT_WCHAR_OBFUS( 'C'), DRM_INIT_WCHAR_OBFUS( 'O'), DRM_INIT_WCHAR_OBFUS( 'C'), DRM_INIT_WCHAR_OBFUS( 'K'), DRM_INIT_WCHAR_OBFUS( 'T'), DRM_INIT_WCHAR_OBFUS( 'A'), DRM_INIT_WCHAR_OBFUS( 'I'), DRM_INIT_WCHAR_OBFUS( 'L'), DRM_INIT_WCHAR_OBFUS( '\0') };
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagAESCTR                             [] =  { DRM_INIT_WCHAR_OBFUS( 'A'), DRM_INIT_WCHAR_OBFUS( 'E'), DRM_INIT_WCHAR_OBFUS( 'S'), DRM_INIT_WCHAR_OBFUS( 'C'), DRM_INIT_WCHAR_OBFUS( 'T'), DRM_INIT_WCHAR_OBFUS( 'R'), DRM_INIT_WCHAR_OBFUS( '\0') };
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagLwstomAttributes                   [] =  { DRM_INIT_WCHAR_OBFUS( 'C'), DRM_INIT_WCHAR_OBFUS( 'U'), DRM_INIT_WCHAR_OBFUS( 'S'), DRM_INIT_WCHAR_OBFUS( 'T'), DRM_INIT_WCHAR_OBFUS( 'O'), DRM_INIT_WCHAR_OBFUS( 'M'), DRM_INIT_WCHAR_OBFUS( 'A'), DRM_INIT_WCHAR_OBFUS( 'T'), DRM_INIT_WCHAR_OBFUS( 'T'), DRM_INIT_WCHAR_OBFUS( 'R'), DRM_INIT_WCHAR_OBFUS( 'I'), DRM_INIT_WCHAR_OBFUS( 'B'), DRM_INIT_WCHAR_OBFUS( 'U'), DRM_INIT_WCHAR_OBFUS( 'T'), DRM_INIT_WCHAR_OBFUS( 'E'), DRM_INIT_WCHAR_OBFUS( 'S'), DRM_INIT_WCHAR_OBFUS( '\0') };
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagONDEMAND                           [] =  { DRM_INIT_WCHAR_OBFUS( 'O'), DRM_INIT_WCHAR_OBFUS( 'N'), DRM_INIT_WCHAR_OBFUS( 'D'), DRM_INIT_WCHAR_OBFUS( 'E'), DRM_INIT_WCHAR_OBFUS( 'M'), DRM_INIT_WCHAR_OBFUS( 'A'), DRM_INIT_WCHAR_OBFUS( 'N'), DRM_INIT_WCHAR_OBFUS( 'D'), DRM_INIT_WCHAR_OBFUS( '\0') };
DRM_STR_CONST        DRM_WCHAR      g_rgwchEmptyWRMHeaderV4_1                    [] =  { DRM_INIT_WCHAR_OBFUS( '<'), DRM_INIT_WCHAR_OBFUS( 'W'), DRM_INIT_WCHAR_OBFUS( 'R'), DRM_INIT_WCHAR_OBFUS( 'M'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'H'), DRM_INIT_WCHAR_OBFUS( 'E'), DRM_INIT_WCHAR_OBFUS( 'A'), DRM_INIT_WCHAR_OBFUS( 'D'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'E'), DRM_INIT_WCHAR_OBFUS( 'R'), DRM_INIT_WCHAR_OBFUS( ' '), DRM_INIT_WCHAR_OBFUS( 'x'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'm'), DRM_INIT_WCHAR_OBFUS( 'l'), DRM_INIT_WCHAR_OBFUS( 'n'), DRM_INIT_WCHAR_OBFUS( 's'),
                                                                                         DRM_INIT_WCHAR_OBFUS( '='), DRM_INIT_WCHAR_OBFUS( '"'), DRM_INIT_WCHAR_OBFUS( 'h'), DRM_INIT_WCHAR_OBFUS( 't'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 't'), DRM_INIT_WCHAR_OBFUS( 'p'), DRM_INIT_WCHAR_OBFUS( ':'), DRM_INIT_WCHAR_OBFUS( '/'),
                                                                                         DRM_INIT_WCHAR_OBFUS( '/'), DRM_INIT_WCHAR_OBFUS( 's'), DRM_INIT_WCHAR_OBFUS( 'c'), DRM_INIT_WCHAR_OBFUS( 'h'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'e'), DRM_INIT_WCHAR_OBFUS( 'm'), DRM_INIT_WCHAR_OBFUS( 'a'), DRM_INIT_WCHAR_OBFUS( 's'),
                                                                                         DRM_INIT_WCHAR_OBFUS( '.'), DRM_INIT_WCHAR_OBFUS( 'm'), DRM_INIT_WCHAR_OBFUS( 'i'), DRM_INIT_WCHAR_OBFUS( 'c'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'r'), DRM_INIT_WCHAR_OBFUS( 'o'), DRM_INIT_WCHAR_OBFUS( 's'), DRM_INIT_WCHAR_OBFUS( 'o'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'f'), DRM_INIT_WCHAR_OBFUS( 't'), DRM_INIT_WCHAR_OBFUS( '.'), DRM_INIT_WCHAR_OBFUS( 'c'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'o'), DRM_INIT_WCHAR_OBFUS( 'm'), DRM_INIT_WCHAR_OBFUS( '/'), DRM_INIT_WCHAR_OBFUS( 'D'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'R'), DRM_INIT_WCHAR_OBFUS( 'M'), DRM_INIT_WCHAR_OBFUS( '/'), DRM_INIT_WCHAR_OBFUS( '2'),
                                                                                         DRM_INIT_WCHAR_OBFUS( '0'), DRM_INIT_WCHAR_OBFUS( '0'), DRM_INIT_WCHAR_OBFUS( '7'), DRM_INIT_WCHAR_OBFUS( '/'),
                                                                                         DRM_INIT_WCHAR_OBFUS( '0'), DRM_INIT_WCHAR_OBFUS( '3'), DRM_INIT_WCHAR_OBFUS( '/'), DRM_INIT_WCHAR_OBFUS( 'P'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'l'), DRM_INIT_WCHAR_OBFUS( 'a'), DRM_INIT_WCHAR_OBFUS( 'y'), DRM_INIT_WCHAR_OBFUS( 'R'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'e'), DRM_INIT_WCHAR_OBFUS( 'a'), DRM_INIT_WCHAR_OBFUS( 'd'), DRM_INIT_WCHAR_OBFUS( 'y'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'H'), DRM_INIT_WCHAR_OBFUS( 'e'), DRM_INIT_WCHAR_OBFUS( 'a'), DRM_INIT_WCHAR_OBFUS( 'd'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'e'), DRM_INIT_WCHAR_OBFUS( 'r'), DRM_INIT_WCHAR_OBFUS( '"'), DRM_INIT_WCHAR_OBFUS( ' '),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'v'), DRM_INIT_WCHAR_OBFUS( 'e'), DRM_INIT_WCHAR_OBFUS( 'r'), DRM_INIT_WCHAR_OBFUS( 's'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'i'), DRM_INIT_WCHAR_OBFUS( 'o'), DRM_INIT_WCHAR_OBFUS( 'n'), DRM_INIT_WCHAR_OBFUS( '='),
                                                                                         DRM_INIT_WCHAR_OBFUS( '"'), DRM_INIT_WCHAR_OBFUS( '4'), DRM_INIT_WCHAR_OBFUS( '.'), DRM_INIT_WCHAR_OBFUS( '1'),
                                                                                         DRM_INIT_WCHAR_OBFUS( '.'), DRM_INIT_WCHAR_OBFUS( '0'), DRM_INIT_WCHAR_OBFUS( '.'), DRM_INIT_WCHAR_OBFUS( '0'),
                                                                                         DRM_INIT_WCHAR_OBFUS( '"'), DRM_INIT_WCHAR_OBFUS( '>'), DRM_INIT_WCHAR_OBFUS( '<'), DRM_INIT_WCHAR_OBFUS( '/'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'W'), DRM_INIT_WCHAR_OBFUS( 'R'), DRM_INIT_WCHAR_OBFUS( 'M'), DRM_INIT_WCHAR_OBFUS( 'H'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'E'), DRM_INIT_WCHAR_OBFUS( 'A'), DRM_INIT_WCHAR_OBFUS( 'D'), DRM_INIT_WCHAR_OBFUS( 'E'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'R'), DRM_INIT_WCHAR_OBFUS( '>'), DRM_INIT_WCHAR_OBFUS( '\0') };
DRM_STR_CONST        DRM_WCHAR      g_rgwchEmptyWRMHeaderV4_2                    [] =  { DRM_INIT_WCHAR_OBFUS( '<'), DRM_INIT_WCHAR_OBFUS( 'W'), DRM_INIT_WCHAR_OBFUS( 'R'), DRM_INIT_WCHAR_OBFUS( 'M'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'H'), DRM_INIT_WCHAR_OBFUS( 'E'), DRM_INIT_WCHAR_OBFUS( 'A'), DRM_INIT_WCHAR_OBFUS( 'D'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'E'), DRM_INIT_WCHAR_OBFUS( 'R'), DRM_INIT_WCHAR_OBFUS( ' '), DRM_INIT_WCHAR_OBFUS( 'x'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'm'), DRM_INIT_WCHAR_OBFUS( 'l'), DRM_INIT_WCHAR_OBFUS( 'n'), DRM_INIT_WCHAR_OBFUS( 's'),
                                                                                         DRM_INIT_WCHAR_OBFUS( '='), DRM_INIT_WCHAR_OBFUS( '"'), DRM_INIT_WCHAR_OBFUS( 'h'), DRM_INIT_WCHAR_OBFUS( 't'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 't'), DRM_INIT_WCHAR_OBFUS( 'p'), DRM_INIT_WCHAR_OBFUS( ':'), DRM_INIT_WCHAR_OBFUS( '/'),
                                                                                         DRM_INIT_WCHAR_OBFUS( '/'), DRM_INIT_WCHAR_OBFUS( 's'), DRM_INIT_WCHAR_OBFUS( 'c'), DRM_INIT_WCHAR_OBFUS( 'h'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'e'), DRM_INIT_WCHAR_OBFUS( 'm'), DRM_INIT_WCHAR_OBFUS( 'a'), DRM_INIT_WCHAR_OBFUS( 's'),
                                                                                         DRM_INIT_WCHAR_OBFUS( '.'), DRM_INIT_WCHAR_OBFUS( 'm'), DRM_INIT_WCHAR_OBFUS( 'i'), DRM_INIT_WCHAR_OBFUS( 'c'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'r'), DRM_INIT_WCHAR_OBFUS( 'o'), DRM_INIT_WCHAR_OBFUS( 's'), DRM_INIT_WCHAR_OBFUS( 'o'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'f'), DRM_INIT_WCHAR_OBFUS( 't'), DRM_INIT_WCHAR_OBFUS( '.'), DRM_INIT_WCHAR_OBFUS( 'c'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'o'), DRM_INIT_WCHAR_OBFUS( 'm'), DRM_INIT_WCHAR_OBFUS( '/'), DRM_INIT_WCHAR_OBFUS( 'D'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'R'), DRM_INIT_WCHAR_OBFUS( 'M'), DRM_INIT_WCHAR_OBFUS( '/'), DRM_INIT_WCHAR_OBFUS( '2'),
                                                                                         DRM_INIT_WCHAR_OBFUS( '0'), DRM_INIT_WCHAR_OBFUS( '0'), DRM_INIT_WCHAR_OBFUS( '7'), DRM_INIT_WCHAR_OBFUS( '/'),
                                                                                         DRM_INIT_WCHAR_OBFUS( '0'), DRM_INIT_WCHAR_OBFUS( '3'), DRM_INIT_WCHAR_OBFUS( '/'), DRM_INIT_WCHAR_OBFUS( 'P'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'l'), DRM_INIT_WCHAR_OBFUS( 'a'), DRM_INIT_WCHAR_OBFUS( 'y'), DRM_INIT_WCHAR_OBFUS( 'R'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'e'), DRM_INIT_WCHAR_OBFUS( 'a'), DRM_INIT_WCHAR_OBFUS( 'd'), DRM_INIT_WCHAR_OBFUS( 'y'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'H'), DRM_INIT_WCHAR_OBFUS( 'e'), DRM_INIT_WCHAR_OBFUS( 'a'), DRM_INIT_WCHAR_OBFUS( 'd'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'e'), DRM_INIT_WCHAR_OBFUS( 'r'), DRM_INIT_WCHAR_OBFUS( '"'), DRM_INIT_WCHAR_OBFUS( ' '),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'v'), DRM_INIT_WCHAR_OBFUS( 'e'), DRM_INIT_WCHAR_OBFUS( 'r'), DRM_INIT_WCHAR_OBFUS( 's'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'i'), DRM_INIT_WCHAR_OBFUS( 'o'), DRM_INIT_WCHAR_OBFUS( 'n'), DRM_INIT_WCHAR_OBFUS( '='),
                                                                                         DRM_INIT_WCHAR_OBFUS( '"'), DRM_INIT_WCHAR_OBFUS( '4'), DRM_INIT_WCHAR_OBFUS( '.'), DRM_INIT_WCHAR_OBFUS( '2'),
                                                                                         DRM_INIT_WCHAR_OBFUS( '.'), DRM_INIT_WCHAR_OBFUS( '0'), DRM_INIT_WCHAR_OBFUS( '.'), DRM_INIT_WCHAR_OBFUS( '0'),
                                                                                         DRM_INIT_WCHAR_OBFUS( '"'), DRM_INIT_WCHAR_OBFUS( '>'), DRM_INIT_WCHAR_OBFUS( '<'), DRM_INIT_WCHAR_OBFUS( '/'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'W'), DRM_INIT_WCHAR_OBFUS( 'R'), DRM_INIT_WCHAR_OBFUS( 'M'), DRM_INIT_WCHAR_OBFUS( 'H'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'E'), DRM_INIT_WCHAR_OBFUS( 'A'), DRM_INIT_WCHAR_OBFUS( 'D'), DRM_INIT_WCHAR_OBFUS( 'E'),
                                                                                         DRM_INIT_WCHAR_OBFUS( 'R'), DRM_INIT_WCHAR_OBFUS( '>'), DRM_INIT_WCHAR_OBFUS( '\0') };
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagAction                             [] =  { DRM_INIT_WCHAR_OBFUS('A'),  DRM_INIT_WCHAR_OBFUS('C'),  DRM_INIT_WCHAR_OBFUS('T'),  DRM_INIT_WCHAR_OBFUS('I'),  DRM_INIT_WCHAR_OBFUS('O'),  DRM_INIT_WCHAR_OBFUS('N'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagSelwrityVersion                    [] =  { DRM_INIT_WCHAR_OBFUS('S'),  DRM_INIT_WCHAR_OBFUS('E'),  DRM_INIT_WCHAR_OBFUS('C'),  DRM_INIT_WCHAR_OBFUS('U'),  DRM_INIT_WCHAR_OBFUS('R'),  DRM_INIT_WCHAR_OBFUS('I'),  DRM_INIT_WCHAR_OBFUS('T'),  DRM_INIT_WCHAR_OBFUS('Y'),  DRM_INIT_WCHAR_OBFUS('V'),  DRM_INIT_WCHAR_OBFUS('E'),  DRM_INIT_WCHAR_OBFUS('R'),  DRM_INIT_WCHAR_OBFUS('S'),  DRM_INIT_WCHAR_OBFUS('I'),  DRM_INIT_WCHAR_OBFUS('O'),  DRM_INIT_WCHAR_OBFUS('N'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagMID                                [] =  { DRM_INIT_WCHAR_OBFUS('M'),  DRM_INIT_WCHAR_OBFUS('I'),  DRM_INIT_WCHAR_OBFUS('D'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagTID                                [] =  { DRM_INIT_WCHAR_OBFUS('T'),  DRM_INIT_WCHAR_OBFUS('I'),  DRM_INIT_WCHAR_OBFUS('D'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagURL                                [] =  { DRM_INIT_WCHAR_OBFUS('U'),  DRM_INIT_WCHAR_OBFUS('R'),  DRM_INIT_WCHAR_OBFUS('L'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchLabelValue                            [] =  { DRM_INIT_WCHAR_OBFUS('v'),  DRM_INIT_WCHAR_OBFUS('a'),  DRM_INIT_WCHAR_OBFUS('l'),  DRM_INIT_WCHAR_OBFUS('u'),  DRM_INIT_WCHAR_OBFUS('e'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchChallenge                             [] =  { DRM_INIT_WCHAR_OBFUS('c'),  DRM_INIT_WCHAR_OBFUS('h'),  DRM_INIT_WCHAR_OBFUS('a'),  DRM_INIT_WCHAR_OBFUS('l'),  DRM_INIT_WCHAR_OBFUS('l'),  DRM_INIT_WCHAR_OBFUS('e'),  DRM_INIT_WCHAR_OBFUS('n'),  DRM_INIT_WCHAR_OBFUS('g'),  DRM_INIT_WCHAR_OBFUS('e'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagCertificateChain                   [] =  { DRM_INIT_WCHAR_OBFUS('C'),  DRM_INIT_WCHAR_OBFUS('E'),  DRM_INIT_WCHAR_OBFUS('R'),  DRM_INIT_WCHAR_OBFUS('T'),  DRM_INIT_WCHAR_OBFUS('I'),  DRM_INIT_WCHAR_OBFUS('F'),  DRM_INIT_WCHAR_OBFUS('I'),  DRM_INIT_WCHAR_OBFUS('C'),  DRM_INIT_WCHAR_OBFUS('A'),  DRM_INIT_WCHAR_OBFUS('T'),  DRM_INIT_WCHAR_OBFUS('E'),  DRM_INIT_WCHAR_OBFUS('C'),  DRM_INIT_WCHAR_OBFUS('H'),  DRM_INIT_WCHAR_OBFUS('A'),  DRM_INIT_WCHAR_OBFUS('I'),  DRM_INIT_WCHAR_OBFUS('N'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagPrivateKey                         [] =  { DRM_INIT_WCHAR_OBFUS('P'),  DRM_INIT_WCHAR_OBFUS('R'),  DRM_INIT_WCHAR_OBFUS('I'),  DRM_INIT_WCHAR_OBFUS('V'),  DRM_INIT_WCHAR_OBFUS('A'),  DRM_INIT_WCHAR_OBFUS('T'),  DRM_INIT_WCHAR_OBFUS('E'),  DRM_INIT_WCHAR_OBFUS('K'),  DRM_INIT_WCHAR_OBFUS('E'),  DRM_INIT_WCHAR_OBFUS('Y'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTemplate                              [] =  { DRM_INIT_WCHAR_OBFUS('T'),  DRM_INIT_WCHAR_OBFUS('E'),  DRM_INIT_WCHAR_OBFUS('M'),  DRM_INIT_WCHAR_OBFUS('P'),  DRM_INIT_WCHAR_OBFUS('L'),  DRM_INIT_WCHAR_OBFUS('A'),  DRM_INIT_WCHAR_OBFUS('T'),  DRM_INIT_WCHAR_OBFUS('E'),  DRM_INIT_WCHAR_OBFUS('\0')};
DRM_STR_CONST        DRM_WCHAR      g_rgwchTagDataSecVerPlatform                 [] =  { DRM_INIT_WCHAR_OBFUS('P'),  DRM_INIT_WCHAR_OBFUS('L'),  DRM_INIT_WCHAR_OBFUS('A'),  DRM_INIT_WCHAR_OBFUS('T'),  DRM_INIT_WCHAR_OBFUS('F'),  DRM_INIT_WCHAR_OBFUS('O'),  DRM_INIT_WCHAR_OBFUS('R'),  DRM_INIT_WCHAR_OBFUS('M'),  DRM_INIT_WCHAR_OBFUS('_'),  DRM_INIT_WCHAR_OBFUS('I'),  DRM_INIT_WCHAR_OBFUS('D'),  DRM_INIT_WCHAR_OBFUS('\0')};

/* Shared Certificate tags */
DRM_STR_CONST        DRM_CHAR  g_rgchAttributeAlgorithm        [] = { DRM_INIT_CHAR_OBFUS('A'), DRM_INIT_CHAR_OBFUS('l'), DRM_INIT_CHAR_OBFUS('g'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('h'), DRM_INIT_CHAR_OBFUS('m'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchAttributeAlgorithm_LEN 9
DRM_STR_CONST        DRM_CHAR  g_rgchAttributeVersionWMDRM     [] = { DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS(':'), DRM_INIT_CHAR_OBFUS('V'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('n'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchAttributeVersionWMDRM_LEN 9
DRM_STR_CONST        DRM_CHAR  g_rgchKeyUsageSignCert          [] = { DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS(':'), DRM_INIT_CHAR_OBFUS('S'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('g'), DRM_INIT_CHAR_OBFUS('n'), DRM_INIT_CHAR_OBFUS('C'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('f'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchKeyUsageSignCert_LEN 17
DRM_STR_CONST        DRM_CHAR  g_rgchKeyUsageEncryptKey        [] = { DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS(':'), DRM_INIT_CHAR_OBFUS('E'), DRM_INIT_CHAR_OBFUS('n'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('y'), DRM_INIT_CHAR_OBFUS('p'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('K'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('y'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchKeyUsageEncryptKey_LEN 12
DRM_STR_CONST        DRM_CHAR  g_rgchOne                       [] = { DRM_INIT_CHAR_OBFUS('1'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchOne_LEN 1
DRM_STR_CONST        DRM_CHAR  g_rgchPrefixManufacturer        [] = { DRM_INIT_CHAR_OBFUS('x'), DRM_INIT_CHAR_OBFUS('m'), DRM_INIT_CHAR_OBFUS('l'), DRM_INIT_CHAR_OBFUS('n'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS(':'), DRM_INIT_CHAR_OBFUS('f'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchPrefixManufacturer_LEN 7
DRM_STR_CONST        DRM_CHAR  g_rgchTagCanonicalization       [] = { DRM_INIT_CHAR_OBFUS('C'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('n'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('n'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('l'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('z'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('n'), DRM_INIT_CHAR_OBFUS('M'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('h'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('d'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchTagCanonicalization_LEN 22
DRM_STR_CONST        DRM_CHAR  g_rgchTagCertificateCollection  [] = { DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS(':'), DRM_INIT_CHAR_OBFUS('C'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('f'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('C'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('l'), DRM_INIT_CHAR_OBFUS('l'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('n'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchTagCertificateCollection_LEN 23
DRM_STR_CONST        DRM_CHAR  g_rgchTagDigestMethod           [] = { DRM_INIT_CHAR_OBFUS('D'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('g'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('M'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('h'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('d'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchTagDigestMethod_LEN 12
DRM_STR_CONST        DRM_CHAR  g_rgchURIDSigSHA1               [] = { DRM_INIT_CHAR_OBFUS('h'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('p'), DRM_INIT_CHAR_OBFUS(':'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('w'), DRM_INIT_CHAR_OBFUS('w'), DRM_INIT_CHAR_OBFUS('w'), DRM_INIT_CHAR_OBFUS('.'), DRM_INIT_CHAR_OBFUS('w'), DRM_INIT_CHAR_OBFUS('3'), DRM_INIT_CHAR_OBFUS('.'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('g'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('2'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('9'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('x'), DRM_INIT_CHAR_OBFUS('m'), DRM_INIT_CHAR_OBFUS('l'), DRM_INIT_CHAR_OBFUS('d'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('g'), DRM_INIT_CHAR_OBFUS('#'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('h'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('1'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchURIDSigSHA1_LEN 38
DRM_STR_CONST        DRM_CHAR  g_rgchTagDigestValue            [] = { DRM_INIT_CHAR_OBFUS('D'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('g'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('V'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('l'), DRM_INIT_CHAR_OBFUS('u'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchTagDigestValue_LEN 11
DRM_STR_CONST        DRM_CHAR  g_rgchTagSignatureValue         [] = { DRM_INIT_CHAR_OBFUS('S'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('g'), DRM_INIT_CHAR_OBFUS('n'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('u'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('V'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('l'), DRM_INIT_CHAR_OBFUS('u'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchTagSignatureValue_LEN 14
DRM_STR_CONST        DRM_CHAR  g_rgchTagKeyInfo                [] = { DRM_INIT_CHAR_OBFUS('K'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('y'), DRM_INIT_CHAR_OBFUS('I'), DRM_INIT_CHAR_OBFUS('n'), DRM_INIT_CHAR_OBFUS('f'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchTagKeyInfo_LEN 7
DRM_STR_CONST        DRM_CHAR  g_rgchTagPublicKey              [] = { DRM_INIT_CHAR_OBFUS('P'), DRM_INIT_CHAR_OBFUS('u'), DRM_INIT_CHAR_OBFUS('b'), DRM_INIT_CHAR_OBFUS('l'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('K'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('y'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchTagPublicKey_LEN 9
DRM_STR_CONST        DRM_CHAR  g_rgchTagPrivateKey             [] = { DRM_INIT_CHAR_OBFUS('P'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('v'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('K'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('y'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchTagPrivateKey_LEN 10
DRM_STR_CONST        DRM_CHAR  g_rgchTagKeyValue               [] = { DRM_INIT_CHAR_OBFUS('K'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('y'), DRM_INIT_CHAR_OBFUS('V'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('l'), DRM_INIT_CHAR_OBFUS('u'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchTagKeyValue_LEN 8
DRM_STR_CONST        DRM_CHAR  g_rgchTagRSAKeyValue            [] = { DRM_INIT_CHAR_OBFUS('R'), DRM_INIT_CHAR_OBFUS('S'), DRM_INIT_CHAR_OBFUS('A'), DRM_INIT_CHAR_OBFUS('K'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('y'), DRM_INIT_CHAR_OBFUS('V'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('l'), DRM_INIT_CHAR_OBFUS('u'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchTagRSAKeyValue_LEN 11
DRM_STR_CONST        DRM_CHAR  g_rgchTagModulus                [] = { DRM_INIT_CHAR_OBFUS('M'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('d'), DRM_INIT_CHAR_OBFUS('u'), DRM_INIT_CHAR_OBFUS('l'), DRM_INIT_CHAR_OBFUS('u'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchTagModulus_LEN 7
DRM_STR_CONST        DRM_CHAR  g_rgchTagExponent               [] = { DRM_INIT_CHAR_OBFUS('E'), DRM_INIT_CHAR_OBFUS('x'), DRM_INIT_CHAR_OBFUS('p'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('n'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('n'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchTagExponent_LEN 8
DRM_STR_CONST        DRM_CHAR  g_rgchTagManufacturerName       [] = { DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS(':'), DRM_INIT_CHAR_OBFUS('M'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('n'), DRM_INIT_CHAR_OBFUS('u'), DRM_INIT_CHAR_OBFUS('f'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('u'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('N'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('m'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchTagManufacturerName_LEN 18
DRM_STR_CONST        DRM_CHAR  g_rgchTagManufacturerData       [] = { DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS(':'), DRM_INIT_CHAR_OBFUS('M'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('n'), DRM_INIT_CHAR_OBFUS('u'), DRM_INIT_CHAR_OBFUS('f'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('u'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('D'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchTagManufacturerData_LEN 18
DRM_STR_CONST        DRM_CHAR  g_rgchURIRSASHA1                [] = { DRM_INIT_CHAR_OBFUS('h'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('p'), DRM_INIT_CHAR_OBFUS(':'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('w'), DRM_INIT_CHAR_OBFUS('w'), DRM_INIT_CHAR_OBFUS('w'), DRM_INIT_CHAR_OBFUS('.'), DRM_INIT_CHAR_OBFUS('w'), DRM_INIT_CHAR_OBFUS('3'), DRM_INIT_CHAR_OBFUS('.'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('g'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('2'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('9'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('x'), DRM_INIT_CHAR_OBFUS('m'), DRM_INIT_CHAR_OBFUS('l'), DRM_INIT_CHAR_OBFUS('d'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('g'), DRM_INIT_CHAR_OBFUS('#'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('-'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('h'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('1'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchURIRSASHA1_LEN 42
DRM_STR_CONST        DRM_CHAR  g_rgchURIRSASHA1_Old            [] = { DRM_INIT_CHAR_OBFUS('h'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('p'), DRM_INIT_CHAR_OBFUS(':'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('h'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('m'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('.'), DRM_INIT_CHAR_OBFUS('m'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('f'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('.'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('m'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('D'), DRM_INIT_CHAR_OBFUS('R'), DRM_INIT_CHAR_OBFUS('M'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('2'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('4'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('2'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('C'), DRM_INIT_CHAR_OBFUS('E'), DRM_INIT_CHAR_OBFUS('R'), DRM_INIT_CHAR_OBFUS('T'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('R'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('-'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('h'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('1'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchURIRSASHA1_Old_LEN 54
DRM_STR_CONST        DRM_CHAR  g_rgchTagReference              [] = { DRM_INIT_CHAR_OBFUS('R'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('f'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('n'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchTagReference_LEN 9
DRM_STR_CONST        DRM_CHAR  g_rgchTagTransforms             [] = { DRM_INIT_CHAR_OBFUS('T'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('n'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('f'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('m'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchTagTransforms_LEN 10
DRM_STR_CONST        DRM_CHAR  g_rgchTagTransform              [] = { DRM_INIT_CHAR_OBFUS('T'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('n'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('f'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('m'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchTagTransform_LEN 9
DRM_STR_CONST        DRM_CHAR  g_rgchURITransformMSCert        [] = { DRM_INIT_CHAR_OBFUS('h'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('p'), DRM_INIT_CHAR_OBFUS(':'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('w'), DRM_INIT_CHAR_OBFUS('w'), DRM_INIT_CHAR_OBFUS('w'), DRM_INIT_CHAR_OBFUS('.'), DRM_INIT_CHAR_OBFUS('m'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('f'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('.'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('m'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('D'), DRM_INIT_CHAR_OBFUS('R'), DRM_INIT_CHAR_OBFUS('M'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('C'), DRM_INIT_CHAR_OBFUS('E'), DRM_INIT_CHAR_OBFUS('R'), DRM_INIT_CHAR_OBFUS('T'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('v'), DRM_INIT_CHAR_OBFUS('2'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('D'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchURITransformMSCert_LEN 41
DRM_STR_CONST        DRM_CHAR  g_rgchURITransformMSCertColl    [] = { DRM_INIT_CHAR_OBFUS('h'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('p'), DRM_INIT_CHAR_OBFUS(':'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('h'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('m'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('.'), DRM_INIT_CHAR_OBFUS('m'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('f'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('.'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('m'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('D'), DRM_INIT_CHAR_OBFUS('R'), DRM_INIT_CHAR_OBFUS('M'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('2'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('4'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('2'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchURITransformMSCertColl_LEN 45
DRM_STR_CONST        DRM_CHAR  g_rgchURITransformMSCert_Old    [] = { DRM_INIT_CHAR_OBFUS('h'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('p'), DRM_INIT_CHAR_OBFUS(':'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('h'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('m'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('.'), DRM_INIT_CHAR_OBFUS('m'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('f'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('.'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('m'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('D'), DRM_INIT_CHAR_OBFUS('R'), DRM_INIT_CHAR_OBFUS('M'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('2'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('4'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('2'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('C'), DRM_INIT_CHAR_OBFUS('E'), DRM_INIT_CHAR_OBFUS('R'), DRM_INIT_CHAR_OBFUS('T'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('D'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchURITransformMSCert_Old_LEN 50
DRM_STR_CONST        DRM_CHAR  g_rgchURITransformC14N          [] = { DRM_INIT_CHAR_OBFUS('h'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('p'), DRM_INIT_CHAR_OBFUS(':'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('w'), DRM_INIT_CHAR_OBFUS('w'), DRM_INIT_CHAR_OBFUS('w'), DRM_INIT_CHAR_OBFUS('.'), DRM_INIT_CHAR_OBFUS('w'), DRM_INIT_CHAR_OBFUS('3'), DRM_INIT_CHAR_OBFUS('.'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('g'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('T'), DRM_INIT_CHAR_OBFUS('R'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('2'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('1'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('R'), DRM_INIT_CHAR_OBFUS('E'), DRM_INIT_CHAR_OBFUS('C'), DRM_INIT_CHAR_OBFUS('-'), DRM_INIT_CHAR_OBFUS('x'), DRM_INIT_CHAR_OBFUS('m'), DRM_INIT_CHAR_OBFUS('l'), DRM_INIT_CHAR_OBFUS('-'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('1'), DRM_INIT_CHAR_OBFUS('4'), DRM_INIT_CHAR_OBFUS('n'), DRM_INIT_CHAR_OBFUS('-'), DRM_INIT_CHAR_OBFUS('2'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('1'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('3'), DRM_INIT_CHAR_OBFUS('1'), DRM_INIT_CHAR_OBFUS('5'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchURITransformC14N_LEN 47
DRM_STR_CONST        DRM_CHAR  g_rgchTagWMDRMCertificate       [] = { DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS(':'), DRM_INIT_CHAR_OBFUS('C'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('f'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchTagWMDRMCertificate_LEN 13
DRM_STR_CONST        DRM_CHAR  g_rgchTagDataWMDRM              [] = { DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS(':'), DRM_INIT_CHAR_OBFUS('D'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchTagDataWMDRM_LEN 6
DRM_STR_CONST        DRM_CHAR  g_rgchTagWMDRMCertPublicKey     [] = { DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS(':'), DRM_INIT_CHAR_OBFUS('P'), DRM_INIT_CHAR_OBFUS('u'), DRM_INIT_CHAR_OBFUS('b'), DRM_INIT_CHAR_OBFUS('l'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('K'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('y'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchTagWMDRMCertPublicKey_LEN 11
DRM_STR_CONST        DRM_CHAR  g_rgchTagWMDRMCertSelwrityVersion[]= { DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS(':'), DRM_INIT_CHAR_OBFUS('S'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('u'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('y'), DRM_INIT_CHAR_OBFUS('V'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('n'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchTagWMDRMCertSelwrityVersion_LEN 17
DRM_STR_CONST        DRM_CHAR  g_rgchTagWMDRMCertSelwrityLevel [] = { DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS(':'), DRM_INIT_CHAR_OBFUS('S'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('u'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('y'), DRM_INIT_CHAR_OBFUS('L'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('v'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('l'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchTagWMDRMCertSelwrityLevel_LEN 15
DRM_STR_CONST        DRM_CHAR  g_rgchTagWMDRMCertFeatures      [] = { DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS(':'), DRM_INIT_CHAR_OBFUS('F'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('u'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchTagWMDRMCertFeatures_LEN 10
DRM_STR_CONST        DRM_CHAR  g_rgchTagWMDRMCertKeyUsage      [] = { DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS(':'), DRM_INIT_CHAR_OBFUS('K'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('y'), DRM_INIT_CHAR_OBFUS('U'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('g'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchTagWMDRMCertKeyUsage_LEN 10
DRM_STR_CONST        DRM_CHAR  g_rgchFeaturesWMDRMTransmitter  [] = { DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS(':'), DRM_INIT_CHAR_OBFUS('W'), DRM_INIT_CHAR_OBFUS('M'), DRM_INIT_CHAR_OBFUS('D'), DRM_INIT_CHAR_OBFUS('R'), DRM_INIT_CHAR_OBFUS('M'), DRM_INIT_CHAR_OBFUS('T'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('n'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('m'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchFeaturesWMDRMTransmitter_LEN 18
DRM_STR_CONST        DRM_CHAR  g_rgchFeaturesWMDRMReceiver     [] = { DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS(':'), DRM_INIT_CHAR_OBFUS('W'), DRM_INIT_CHAR_OBFUS('M'), DRM_INIT_CHAR_OBFUS('D'), DRM_INIT_CHAR_OBFUS('R'), DRM_INIT_CHAR_OBFUS('M'), DRM_INIT_CHAR_OBFUS('R'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('v'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchFeaturesWMDRMReceiver_LEN 15
DRM_STR_CONST        DRM_CHAR  g_rgchTagSignature              [] = { DRM_INIT_CHAR_OBFUS('S'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('g'), DRM_INIT_CHAR_OBFUS('n'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('u'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchTagSignature_LEN 9
DRM_STR_CONST        DRM_CHAR  g_rgchTagSignatureMethod        [] = { DRM_INIT_CHAR_OBFUS('S'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('g'), DRM_INIT_CHAR_OBFUS('n'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('u'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('M'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('h'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('d'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchTagSignatureMethod_LEN 15
DRM_STR_CONST        DRM_CHAR  g_rgchTagSignedInfo             [] = { DRM_INIT_CHAR_OBFUS('S'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('g'), DRM_INIT_CHAR_OBFUS('n'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('d'), DRM_INIT_CHAR_OBFUS('I'), DRM_INIT_CHAR_OBFUS('n'), DRM_INIT_CHAR_OBFUS('f'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchTagSignedInfo_LEN 10
DRM_STR_CONST        DRM_CHAR  g_rgchURIC14N                   [] = { DRM_INIT_CHAR_OBFUS('h'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('p'), DRM_INIT_CHAR_OBFUS(':'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('w'), DRM_INIT_CHAR_OBFUS('w'), DRM_INIT_CHAR_OBFUS('w'), DRM_INIT_CHAR_OBFUS('.'), DRM_INIT_CHAR_OBFUS('w'), DRM_INIT_CHAR_OBFUS('3'), DRM_INIT_CHAR_OBFUS('.'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('r'), DRM_INIT_CHAR_OBFUS('g'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('T'), DRM_INIT_CHAR_OBFUS('R'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('2'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('1'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('R'), DRM_INIT_CHAR_OBFUS('E'), DRM_INIT_CHAR_OBFUS('C'), DRM_INIT_CHAR_OBFUS('-'), DRM_INIT_CHAR_OBFUS('x'), DRM_INIT_CHAR_OBFUS('m'), DRM_INIT_CHAR_OBFUS('l'), DRM_INIT_CHAR_OBFUS('-'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('1'), DRM_INIT_CHAR_OBFUS('4'), DRM_INIT_CHAR_OBFUS('n'), DRM_INIT_CHAR_OBFUS('-'), DRM_INIT_CHAR_OBFUS('2'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('1'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('3'), DRM_INIT_CHAR_OBFUS('1'), DRM_INIT_CHAR_OBFUS('5'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchURIC14N_LEN 47
DRM_STR_CONST        DRM_CHAR  g_rgchVersionWMDRM              [] = { DRM_INIT_CHAR_OBFUS('2'), DRM_INIT_CHAR_OBFUS('.'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchVersionWMDRM_LEN 3
DRM_STR_CONST        DRM_CHAR  g_rgchWMDRMCertExponent         [] = { DRM_INIT_CHAR_OBFUS('A'), DRM_INIT_CHAR_OBFUS('Q'), DRM_INIT_CHAR_OBFUS('A'), DRM_INIT_CHAR_OBFUS('B'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchWMDRMCertExponent_LEN 4
DRM_STR_CONST        DRM_CHAR  g_rgchPrefixMicrosoftCert       [] = { DRM_INIT_CHAR_OBFUS('x'), DRM_INIT_CHAR_OBFUS('m'), DRM_INIT_CHAR_OBFUS('l'), DRM_INIT_CHAR_OBFUS('n'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS(':'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchPrefixMicrosoftCert_LEN 7
DRM_STR_CONST        DRM_CHAR  g_rgchMSNDRootPubKeyB64         [] = { DRM_INIT_CHAR_OBFUS('p'), DRM_INIT_CHAR_OBFUS('j'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('W'), DRM_INIT_CHAR_OBFUS('L'), DRM_INIT_CHAR_OBFUS('S'), DRM_INIT_CHAR_OBFUS('T'), DRM_INIT_CHAR_OBFUS('L'), DRM_INIT_CHAR_OBFUS('D'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('n'), DRM_INIT_CHAR_OBFUS('Q'), DRM_INIT_CHAR_OBFUS('G'), DRM_INIT_CHAR_OBFUS('8'), DRM_INIT_CHAR_OBFUS('S'), DRM_INIT_CHAR_OBFUS('h'), DRM_INIT_CHAR_OBFUS('e'), DRM_INIT_CHAR_OBFUS('6'), DRM_INIT_CHAR_OBFUS('Q'), DRM_INIT_CHAR_OBFUS('h'), DRM_INIT_CHAR_OBFUS('k'), DRM_INIT_CHAR_OBFUS('Y'), DRM_INIT_CHAR_OBFUS('b'), DRM_INIT_CHAR_OBFUS('Y'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('9'), DRM_INIT_CHAR_OBFUS('f'), DRM_INIT_CHAR_OBFUS('P'), DRM_INIT_CHAR_OBFUS('Z'), DRM_INIT_CHAR_OBFUS('8'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('H'), DRM_INIT_CHAR_OBFUS('d'), DRM_INIT_CHAR_OBFUS('B'), DRM_INIT_CHAR_OBFUS('1'), DRM_INIT_CHAR_OBFUS('2'), DRM_INIT_CHAR_OBFUS('8'), DRM_INIT_CHAR_OBFUS('Z'), DRM_INIT_CHAR_OBFUS('E'), DRM_INIT_CHAR_OBFUS('T'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('g'), DRM_INIT_CHAR_OBFUS('h'), DRM_INIT_CHAR_OBFUS('n'), DRM_INIT_CHAR_OBFUS('5'), DRM_INIT_CHAR_OBFUS('K'), DRM_INIT_CHAR_OBFUS('H'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('y'), DRM_INIT_CHAR_OBFUS('i'), DRM_INIT_CHAR_OBFUS('n'), DRM_INIT_CHAR_OBFUS('7'), DRM_INIT_CHAR_OBFUS('H'), DRM_INIT_CHAR_OBFUS('k'), DRM_INIT_CHAR_OBFUS('J'), DRM_INIT_CHAR_OBFUS('E'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('P'), DRM_INIT_CHAR_OBFUS('J'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('E'), DRM_INIT_CHAR_OBFUS('g'), DRM_INIT_CHAR_OBFUS('4'), DRM_INIT_CHAR_OBFUS('U'), DRM_INIT_CHAR_OBFUS('d'), DRM_INIT_CHAR_OBFUS('S'), DRM_INIT_CHAR_OBFUS('v'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('K'), DRM_INIT_CHAR_OBFUS('D'), DRM_INIT_CHAR_OBFUS('I'), DRM_INIT_CHAR_OBFUS('Y'), DRM_INIT_CHAR_OBFUS('D'), DRM_INIT_CHAR_OBFUS('j'), DRM_INIT_CHAR_OBFUS('A'), DRM_INIT_CHAR_OBFUS('3'), DRM_INIT_CHAR_OBFUS('E'), DRM_INIT_CHAR_OBFUS('X'), DRM_INIT_CHAR_OBFUS('d'), DRM_INIT_CHAR_OBFUS('6'), DRM_INIT_CHAR_OBFUS('9'), DRM_INIT_CHAR_OBFUS('R'), DRM_INIT_CHAR_OBFUS('3'), DRM_INIT_CHAR_OBFUS('C'), DRM_INIT_CHAR_OBFUS('N'), DRM_INIT_CHAR_OBFUS('2'), DRM_INIT_CHAR_OBFUS('W'), DRM_INIT_CHAR_OBFUS('p'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('Q'), DRM_INIT_CHAR_OBFUS('y'), DRM_INIT_CHAR_OBFUS('O'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('Z'), DRM_INIT_CHAR_OBFUS('P'), DRM_INIT_CHAR_OBFUS('Y'), DRM_INIT_CHAR_OBFUS('W'), DRM_INIT_CHAR_OBFUS('Y'), DRM_INIT_CHAR_OBFUS('p'), DRM_INIT_CHAR_OBFUS('3'), DRM_INIT_CHAR_OBFUS('N'), DRM_INIT_CHAR_OBFUS('X'), DRM_INIT_CHAR_OBFUS('p'), DRM_INIT_CHAR_OBFUS('J'), DRM_INIT_CHAR_OBFUS('7'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('0'), DRM_INIT_CHAR_OBFUS('t'), DRM_INIT_CHAR_OBFUS('K'), DRM_INIT_CHAR_OBFUS('P'), DRM_INIT_CHAR_OBFUS('g'), DRM_INIT_CHAR_OBFUS('I'), DRM_INIT_CHAR_OBFUS('p'), DRM_INIT_CHAR_OBFUS('l'), DRM_INIT_CHAR_OBFUS('z'), DRM_INIT_CHAR_OBFUS('o'), DRM_INIT_CHAR_OBFUS('5'), DRM_INIT_CHAR_OBFUS('w'), DRM_INIT_CHAR_OBFUS('V'), DRM_INIT_CHAR_OBFUS('d'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('6'), DRM_INIT_CHAR_OBFUS('9'), DRM_INIT_CHAR_OBFUS('g'), DRM_INIT_CHAR_OBFUS('7'), DRM_INIT_CHAR_OBFUS('j'), DRM_INIT_CHAR_OBFUS('+'), DRM_INIT_CHAR_OBFUS('j'), DRM_INIT_CHAR_OBFUS('8'), DRM_INIT_CHAR_OBFUS('M'), DRM_INIT_CHAR_OBFUS('6'), DRM_INIT_CHAR_OBFUS('6'), DRM_INIT_CHAR_OBFUS('W'), DRM_INIT_CHAR_OBFUS('7'), DRM_INIT_CHAR_OBFUS('V'), DRM_INIT_CHAR_OBFUS('N'), DRM_INIT_CHAR_OBFUS('m'), DRM_INIT_CHAR_OBFUS('D'), DRM_INIT_CHAR_OBFUS('w'), DRM_INIT_CHAR_OBFUS('a'), DRM_INIT_CHAR_OBFUS('N'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('9'), DRM_INIT_CHAR_OBFUS('m'), DRM_INIT_CHAR_OBFUS('D'), DRM_INIT_CHAR_OBFUS('c'), DRM_INIT_CHAR_OBFUS('1'), DRM_INIT_CHAR_OBFUS('p'), DRM_INIT_CHAR_OBFUS('2'), DRM_INIT_CHAR_OBFUS('+'), DRM_INIT_CHAR_OBFUS('V'), DRM_INIT_CHAR_OBFUS('V'), DRM_INIT_CHAR_OBFUS('M'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('D'), DRM_INIT_CHAR_OBFUS('h'), DRM_INIT_CHAR_OBFUS('O'), DRM_INIT_CHAR_OBFUS('s'), DRM_INIT_CHAR_OBFUS('V'), DRM_INIT_CHAR_OBFUS('/'), DRM_INIT_CHAR_OBFUS('A'), DRM_INIT_CHAR_OBFUS('u'), DRM_INIT_CHAR_OBFUS('6'), DRM_INIT_CHAR_OBFUS('E'), DRM_INIT_CHAR_OBFUS('+'), DRM_INIT_CHAR_OBFUS('E'), DRM_INIT_CHAR_OBFUS('='), DRM_INIT_CHAR_OBFUS('\0')};
#define g_rgchMSNDRootPubKeyB64_LEN 172

/* Misc strings shared across disparate functional areas */
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING g_adstrLicenseRespTag                       = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchLicenseRespTag,    15 );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrGUID                                  = DRM_CREATE_DRM_STRING( g_rgwchGUID );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrFlag                                  = DRM_CREATE_DRM_STRING( g_rgwchFlag );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrSelwreClockNotSet                     = DRM_CREATE_DRM_STRING( g_rgwchSecClockNotSet );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrSelwreClockSet                        = DRM_CREATE_DRM_STRING( g_rgwchSecClockSet );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrSelwreClockNeedsRefresh               = DRM_CREATE_DRM_STRING( g_rgwchSecClockNeedsRefresh );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrExprVarSavedDateTime                  = DRM_CREATE_DRM_STRING( g_rgwchExprVarSavedDateTime );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagMetering                           = DRM_CREATE_DRM_STRING( g_rgwchTagMetering );

/* Rights */
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrWMDRM_RIGHT_NONE                       = DRM_CREATE_DRM_STRING( g_rgwchWMDRM_RIGHT_NONE );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrWMDRM_RIGHT_PLAYBACK                   = DRM_CREATE_DRM_STRING( g_rgwchWMDRM_RIGHT_PLAYBACK );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrWMDRM_RIGHT_COLLABORATIVE_PLAY         = DRM_CREATE_DRM_STRING( g_rgwchWMDRM_RIGHT_COLLABORATIVE_PLAY );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrWMDRM_RIGHT_COPY_TO_CD                 = DRM_CREATE_DRM_STRING( g_rgwchWMDRM_RIGHT_COPY_TO_CD );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrWMDRM_RIGHT_COPY                       = DRM_CREATE_DRM_STRING( g_rgwchWMDRM_RIGHT_COPY );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrWMDRM_RIGHT_CREATE_THUMBNAIL_IMAGE     = DRM_CREATE_DRM_STRING( g_rgwchWMDRM_RIGHT_CREATE_THUMBNAIL_IMAGE );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrWMDRM_RIGHT_MOVE                       = DRM_CREATE_DRM_STRING( g_rgwchWMDRM_RIGHT_MOVE );

/* Script varibles used for license properties. */
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrDRM_LS_COUNT_ATTR                     = DRM_CREATE_DRM_STRING( g_rgwchDRM_LS_COUNT_ATTR );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrDRM_LS_FIRSTUSE_ATTR                  = DRM_CREATE_DRM_STRING( g_rgwchDRM_LS_FIRSTUSE_ATTR );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrDRM_LS_FIRSTSTORE_ATTR                = DRM_CREATE_DRM_STRING( g_rgwchDRM_LS_FIRSTSTORE_ATTR );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrDRM_LS_PLAYCOUNT_ATTR                 = DRM_CREATE_DRM_STRING( g_rgwchDRM_LS_PLAYCOUNT_ATTR );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrDRM_LS_COPYCOUNT_ATTR                 = DRM_CREATE_DRM_STRING( g_rgwchDRM_LS_COPYCOUNT_ATTR );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrDRM_LS_DELETED_ATTR                   = DRM_CREATE_DRM_STRING( g_rgwchDRM_LS_DELETED_ATTR );

/* Shared XML tags */
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrAttributeVersion                      = DRM_CREATE_DRM_STRING( g_rgwchAttributeVersion );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagData                               = DRM_CREATE_DRM_STRING( g_rgwchTagData );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagIndex                              = DRM_CREATE_DRM_STRING( g_rgwchTagIndex );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagPubkey                             = DRM_CREATE_DRM_STRING( g_rgwchTagPubkey );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagValue                              = DRM_CREATE_DRM_STRING( g_rgwchTagValue );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagSignature                          = DRM_CREATE_DRM_STRING( g_rgwchTagSignature );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagHashAlg                            = DRM_CREATE_DRM_STRING( g_rgwchTagHashAlg );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagSignAlg                            = DRM_CREATE_DRM_STRING( g_rgwchTagSignAlg );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrSHA                                   = DRM_CREATE_DRM_STRING( g_rgwchSHA );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrMSDRM                                 = DRM_CREATE_DRM_STRING( g_rgwchMSDRM );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING g_dastrAttributeType                        = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchAttributeType, 4 );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrAttributeType                         = DRM_CREATE_DRM_STRING( g_rgwchAttributeType );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagCertificate                        = DRM_CREATE_DRM_STRING( g_rgwchTagCertificate );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagWrmHeader                          = DRM_CREATE_DRM_STRING( g_rgwchTagWrmHeader );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrAttributeVersion2Value                = DRM_CREATE_DRM_STRING( g_rgwchAttributeVersion2Value );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrAttributeVersion4Value                = DRM_CREATE_DRM_STRING( g_rgwchAttributeVersion4Value );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrAttributeVersion4_1Value              = DRM_CREATE_DRM_STRING( g_rgwchAttributeVersion4_1Value );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrAttributeVersion4_2Value              = DRM_CREATE_DRM_STRING( g_rgwchAttributeVersion4_2Value );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagLAINFO                             = DRM_CREATE_DRM_STRING( g_rgwchTagLAINFO );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagV4DATA                             = DRM_CREATE_DRM_STRING( g_rgwchTagV4DATA );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagLAURL                              = DRM_CREATE_DRM_STRING( g_rgwchTagLAURL );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagLUIURL                             = DRM_CREATE_DRM_STRING( g_rgwchTagLUIURL );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagDSID                               = DRM_CREATE_DRM_STRING( g_rgwchTagDSID );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagDataPubKey                         = DRM_CREATE_DRM_STRING( g_rgwchTagDataPubKey );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagKID                                = DRM_CREATE_DRM_STRING( g_rgwchTagKID );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagKIDS                               = DRM_CREATE_DRM_STRING( g_rgwchTagKIDS );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagChecksum                           = DRM_CREATE_DRM_STRING( g_rgwchTagChecksum );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagAlgID                              = DRM_CREATE_DRM_STRING( g_rgwchTagAlgID );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagUplink                             = DRM_CREATE_DRM_STRING( g_rgwchTagUplink );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagDecryptorSetup                     = DRM_CREATE_DRM_STRING( g_rgwchTagDecryptorSetup );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagProtectInfo                        = DRM_CREATE_DRM_STRING( g_rgwchTagProtectInfo );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagKeyLen                             = DRM_CREATE_DRM_STRING( g_rgwchTagKeyLen );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrKeyLenNodeDataCocktail                = DRM_CREATE_DRM_STRING( g_rgwchTagKeyLenNodeDataCocktail );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrKeyLenNodeDataAESCTR                  = DRM_CREATE_DRM_STRING( g_rgwchTagKeyLenNodeDataAESCTR );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagCOCKTAIL                           = DRM_CREATE_DRM_STRING( g_rgwchTagCOCKTAIL );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagAESCTR                             = DRM_CREATE_DRM_STRING( g_rgwchTagAESCTR );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagLwstomAttributes                   = DRM_CREATE_DRM_STRING( g_rgwchTagLwstomAttributes );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagONDEMAND                           = DRM_CREATE_DRM_STRING( g_rgwchTagONDEMAND );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrEmptyWRMHeaderV4_1                    = DRM_CREATE_DRM_STRING( g_rgwchEmptyWRMHeaderV4_1 );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrEmptyWRMHeaderV4_2                    = DRM_CREATE_DRM_STRING( g_rgwchEmptyWRMHeaderV4_2 );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagAction                             = DRM_CREATE_DRM_STRING( g_rgwchTagAction );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagSelwrityVersion                    = DRM_CREATE_DRM_STRING( g_rgwchTagSelwrityVersion );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagMID                                = DRM_CREATE_DRM_STRING( g_rgwchTagMID );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagTID                                = DRM_CREATE_DRM_STRING( g_rgwchTagTID );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagURL                                = DRM_CREATE_DRM_STRING( g_rgwchTagURL );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrLabelValue                            = DRM_CREATE_DRM_STRING( g_rgwchLabelValue );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrChallenge                             = DRM_CREATE_DRM_STRING( g_rgwchChallenge );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagCertificateChain                   = DRM_CREATE_DRM_STRING( g_rgwchTagCertificateChain );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagPrivateKey                         = DRM_CREATE_DRM_STRING( g_rgwchTagPrivateKey );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTemplate                              = DRM_CREATE_DRM_STRING( g_rgwchTemplate );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_CONST_STRING      g_dstrTagDataSecVerPlatform                 = DRM_CREATE_DRM_STRING( g_rgwchTagDataSecVerPlatform );

/* Shared Certificate tags */
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrAttributeAlgorithm                  = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchAttributeAlgorithm, g_rgchAttributeAlgorithm_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrAttributeVersionWMDRM               = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchAttributeVersionWMDRM, g_rgchAttributeVersionWMDRM_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrKeyUsageSignCert                    = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchKeyUsageSignCert, g_rgchKeyUsageSignCert_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrKeyUsageEncryptKey                  = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchKeyUsageEncryptKey, g_rgchKeyUsageEncryptKey_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrOne                                 = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchOne, g_rgchOne_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrPrefixManufacturer                  = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchPrefixManufacturer, g_rgchPrefixManufacturer_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrTagCanonicalization                 = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchTagCanonicalization, g_rgchTagCanonicalization_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrTagCertificateCollection            = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchTagCertificateCollection, g_rgchTagCertificateCollection_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrTagDigestMethod                     = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchTagDigestMethod, g_rgchTagDigestMethod_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrURIDSigSHA1                         = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchURIDSigSHA1, g_rgchURIDSigSHA1_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrTagDigestValue                      = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchTagDigestValue, g_rgchTagDigestValue_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrTagSignatureValue                   = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchTagSignatureValue, g_rgchTagSignatureValue_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrTagKeyInfo                          = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchTagKeyInfo, g_rgchTagKeyInfo_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrTagPublicKey                        = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchTagPublicKey, g_rgchTagPublicKey_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrTagPrivateKey                       = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchTagPrivateKey, g_rgchTagPrivateKey_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrTagKeyValue                         = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchTagKeyValue, g_rgchTagKeyValue_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrTagRSAKeyValue                      = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchTagRSAKeyValue, g_rgchTagRSAKeyValue_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrTagModulus                          = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchTagModulus, g_rgchTagModulus_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrTagExponent                         = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchTagExponent, g_rgchTagExponent_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrTagManufacturerName                 = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchTagManufacturerName, g_rgchTagManufacturerName_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrTagManufacturerData                 = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchTagManufacturerData, g_rgchTagManufacturerData_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrURIRSASHA1                          = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchURIRSASHA1, g_rgchURIRSASHA1_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrURIRSASHA1_Old                      = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchURIRSASHA1_Old, g_rgchURIRSASHA1_Old_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrTagReference                        = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchTagReference, g_rgchTagReference_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrTagTransforms                       = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchTagTransforms, g_rgchTagTransforms_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrTagTransform                        = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchTagTransform, g_rgchTagTransform_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrURITransformMSCert                  = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchURITransformMSCert, g_rgchURITransformMSCert_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrURITransformMSCertColl              = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchURITransformMSCertColl, g_rgchURITransformMSCertColl_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrURITransformMSCert_Old              = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchURITransformMSCert_Old, g_rgchURITransformMSCert_Old_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrURITransformC14N                    = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchURITransformC14N, g_rgchURITransformC14N_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrTagWMDRMCertificate                 = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchTagWMDRMCertificate, g_rgchTagWMDRMCertificate_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrTagWMDRMData                        = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchTagDataWMDRM, g_rgchTagDataWMDRM_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrTagWMDRMCertPublicKey               = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchTagWMDRMCertPublicKey, g_rgchTagWMDRMCertPublicKey_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrTagWMDRMCertSelwrityVersion         = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchTagWMDRMCertSelwrityVersion, g_rgchTagWMDRMCertSelwrityVersion_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrTagWMDRMCertSelwrityLevel           = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchTagWMDRMCertSelwrityLevel, g_rgchTagWMDRMCertSelwrityLevel_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrTagWMDRMCertFeatures                = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchTagWMDRMCertFeatures, g_rgchTagWMDRMCertFeatures_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrTagWMDRMCertKeyUsage                = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchTagWMDRMCertKeyUsage, g_rgchTagWMDRMCertKeyUsage_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrFeaturesWMDRMTransmitter            = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchFeaturesWMDRMTransmitter, g_rgchFeaturesWMDRMTransmitter_LEN);
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrFeaturesWMDRMReceiver               = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchFeaturesWMDRMReceiver, g_rgchFeaturesWMDRMReceiver_LEN);
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrTagSignature                        = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchTagSignature, g_rgchTagSignature_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrTagSignatureMethod                  = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchTagSignatureMethod, g_rgchTagSignatureMethod_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrTagSignedInfo                       = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchTagSignedInfo, g_rgchTagSignedInfo_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrURIC14N                             = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchURIC14N, g_rgchURIC14N_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrVersionWMDRM                        = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchVersionWMDRM, g_rgchVersionWMDRM_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrWMDRMCertExponent                   = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchWMDRMCertExponent, g_rgchWMDRMCertExponent_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrPrefixMicrosoftCert                 = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchPrefixMicrosoftCert, g_rgchPrefixMicrosoftCert_LEN );
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ANSI_CONST_STRING  g_dastrMSNDRootPubKeyB64                   = DRM_CREATE_DRM_ANSI_STRING_EX( g_rgchMSNDRootPubKeyB64, g_rgchMSNDRootPubKeyB64_LEN );

DRM_GLOBAL_CONST  DRM_DISCARDABLE PUBKEY_P256   g_ECC256MSPlayReadyRootIssuerPubKey = DRM_ECC256_MS_PLAYREADY_ROOT_ISSUER_PUBKEY;

/* Revocation GUIDs */
DRM_DEFINE_GUID( g_guidRevocationTypeRevInfo,
                 0xCCDE5A55, 0xA688, 0x4405, 0xA8, 0x8B, 0xD1, 0x3F, 0x90, 0xD5, 0xBA, 0x3E );

/*
** This GUID is for the PlayReady REVINFO2
** {52D1FF11-D388-4edd-82B7-68EA4C20A16C}
Note that this GUID will also be present in the WMDRM rev-info, in order to
identify the minimum PlayReady Revinfo(V2) version in the WMDRM Revinfo (V1).
This is required to export from WMDRM to PlayReady
*/
DRM_DEFINE_GUID( g_guidRevocationTypeRevInfo2,
                 0x52D1FF11, 0xD388, 0x4EDD, 0x82, 0xB7, 0x68, 0xEA, 0x4C, 0x20, 0xA1, 0x6C );

DRM_DEFINE_GUID( g_guidRevocationTypeWMDRMNET,
                 0xCD75E604, 0x543D, 0x4A9C, 0x9F, 0x09, 0xFE, 0x6D, 0x24, 0xE8, 0xBF, 0x90 );

DRM_DEFINE_GUID( g_guidRevocationTypePlayReadySilverLightRuntime,
                 0x4E9D8C8A, 0xB652, 0x45A7, 0x97, 0x91, 0x69, 0x25, 0xA6, 0xB4, 0x79, 0x1F );

DRM_DEFINE_GUID( g_guidRevocationTypePlayReadySilverLightApplication,
                 0x28082E80, 0xC7A3, 0x40b1, 0x82, 0x56, 0x19, 0xE5, 0xB6, 0xD8, 0x9B, 0x27 );

/* {A2190240-B2CA-40B3-B48D-9BC4C2DC428D} */
DRM_DEFINE_GUID( g_guidRevocationTypeGRL,
                 0xA2190240, 0xB2CA, 0x40B3, 0xB4, 0x8D, 0x9B, 0xC4, 0xC2, 0xDC, 0x42, 0x8D );

/* Actions GUIDs. Used with DRM_LEVL_PerformOperationsXMR() API */
DRM_DEFINE_GUID( DRM_ACTION_PLAY,
                 0x4C3FC9B3, 0x31C2, 0x4FD4, 0x82, 0x4A, 0x04, 0xD4, 0x23, 0x41, 0xA9, 0xD3 );

DRM_DEFINE_GUID( DRM_ACTION_COPY,
                 0xD8C5502C, 0x41B1, 0x4681, 0x8B, 0x61, 0x8A, 0x16, 0x18, 0xA3, 0x1D, 0xA7 );

DRM_DEFINE_GUID( DRM_ACTION_MOVE,
                 0xF73D6BFB, 0x9E70, 0x47FA, 0xAD, 0x1F, 0xEC, 0x15, 0x0F, 0x6C, 0x9A, 0x16 );

DRM_DEFINE_GUID( DRM_ACTION_CREATE_THUMBNAIL,
                 0x8CC2C885, 0xBB0D, 0x4AA2, 0xA3, 0x58, 0x96, 0x37, 0xBB, 0x35, 0x59, 0xA9 );

DRM_DEFINE_GUID( DRM_ACTION_COLLABORATIVE_PLAY,
                 0x6B6B0837, 0x46A4, 0x4015, 0x84, 0xE3, 0x8C, 0x20, 0xB5, 0xA7, 0xCD, 0x3A );

DRM_DEFINE_GUID( DRM_ACTION_COPY_TO_CD,
                 0xEC930B7D, 0x1F2D, 0x4682, 0xA3, 0x8B, 0x8A, 0xB9, 0x77, 0x72, 0x1D, 0x0D );

DRM_DEFINE_GUID( DRM_ACTION_COPY_TO_PC,
                 0xCE480EDE, 0x516B, 0x40B3, 0x90, 0xE1, 0xD6, 0xCF, 0xC4, 0x76, 0x30, 0xC5 );

DRM_DEFINE_GUID( DRM_ACTION_COPY_TO_DEVICE,
                 0x6848955D, 0x516B, 0x4EB0, 0x90, 0xE8, 0x8F, 0x6D, 0x5A, 0x77, 0xB8, 0x5F );

DRM_DEFINE_GUID( DRM_PR_PROTECTION_SYSTEM_ID,
                 0x9A04F079, 0x9840, 0x4286, 0xAB, 0x92, 0xE6, 0x5B, 0xE0, 0x88, 0x5F, 0x95 );

DRM_DEFINE_GUID( g_guidNull,
                 0x00000000, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 );

/*
** PlayEnabler GUID constants
*/
DRM_DEFINE_GUID( DRM_PLAYENABLER_UNKNOWN_OUTPUT,
                 0x786627D8, 0xC2A6, 0x44BE, 0x8F, 0x88, 0x08, 0xAE, 0x25, 0x5B, 0x01, 0xA7 );

DRM_DEFINE_GUID( DRM_PLAYENABLER_CONSTRAINED_RESOLUTION_UNKNOWN_OUTPUT,
                 0xB621D91F, 0xEDCC, 0x4035, 0x8D, 0x4B, 0xDC, 0x71, 0x76, 0x0D, 0x43, 0xE9 );

DRM_DEFINE_GUID( DRM_PLAYENABLER_MIRACAST,
                 0xA340C256, 0x0941, 0x4D4C, 0xAD, 0x1D, 0x0B, 0x67, 0x35, 0xC0, 0xCB, 0x24 );
#endif

#if defined (SEC_COMPILE)
/*
** HDCP Type Restriction
** {ABB2C6F1-E663-4625-A945-972D17B231E7}
*/
DRM_DEFINE_GUID( g_guidHDCPTypeRestriction,
                 0xABB2C6F1, 0xE663, 0x4625, 0xA9, 0x45, 0x97, 0x2D, 0x17, 0xB2, 0x31, 0xE7 );
#endif

#ifdef NONE
/*
** MaxRes Decode
** {9645E831-E01D-4FFF-8342-0A720E3E028F}
*/
DRM_DEFINE_GUID( g_guidMaxResDecode,
                 0x9645E831, 0xE01D, 0x4FFF, 0x83, 0x42, 0x0A, 0x72, 0x0E, 0x3E, 0x02, 0x8F );

/*
** SCMS control bits
** {6D5CFA59-C250-4426-930E-FAC72C8FCFA6}
*/
DRM_DEFINE_GUID( g_guidSCMS,
                 0x6D5CFA59, 0xC250, 0x4426, 0x93, 0x0E, 0xFA, 0xC7, 0x2C, 0x8F, 0xCF, 0xA6 );

/*
** Best Effort CGMS-A
** {225CD36F-F132-49EF-BA8C-C91EA28E4369}
*/
DRM_DEFINE_GUID( g_guidBestEffortCGMSA,
                 0x225CD36F, 0xF132, 0x49EF, 0xBA, 0x8C, 0xC9, 0x1E, 0xA2, 0x8E, 0x43, 0x69 );

/*
** CGMS-A
** {2098DE8D-7DDD-4BAB-96C6-32EBB6FABEA3}
*/
DRM_DEFINE_GUID( g_guidCGMSA,
                 0x2098DE8D, 0x7DDD, 0x4BAB, 0x96, 0xC6, 0x32, 0xEB, 0xB6, 0xFA, 0xBE, 0xA3 );
#endif

#if defined (SEC_COMPILE)
/*
** DigitalToken
** {760AE755-682A-41E0-B1B3-DCDF836A7306}
*/
DRM_DEFINE_GUID( g_guidDigitalOnlyToken,
                 0x760AE755, 0x682A, 0x41E0, 0xB1, 0xB3, 0xDC, 0xDF, 0x83, 0x6A, 0x73, 0x06 );
#endif

#ifdef NONE
/*
** AGCColorStrip
** {C3FD11C6-F8B7-4D20-B008-1DB17D61F2DA}
*/
DRM_DEFINE_GUID( g_guidAGC,
                 0xC3FD11C6, 0xF8B7, 0x4D20, 0xB0, 0x08, 0x1D, 0xB1, 0x7D, 0x61, 0xF2, 0xDA );
#endif

#if defined (SEC_COMPILE)
/*
** MAX_VGA_RESOLUTION:downres
** {D783A191-E083-4BAF-B2DA-E69F910B3772}
*/
DRM_DEFINE_GUID( g_guidVGA_MaxRes,
                 0xD783A191, 0xE083, 0x4BAF, 0xB2, 0xDA, 0xE6, 0x9F, 0x91, 0x0B, 0x37, 0x72 );
#endif
/*
** MAX_COMPONENT_RESOLUTION:downres
** {811C5110-46C8-4C6E-8163-C0482A15D47E}
*/
#ifdef NONE
DRM_DEFINE_GUID( g_guidComponent_MaxRes,
                 0x811C5110, 0x46C8, 0x4C6E, 0x81, 0x63, 0xC0, 0x48, 0x2A, 0x15, 0xD4, 0x7E );

DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_ID g_idNull =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* Versioning constants */
/* Indicates the public root key needed to verify the license server certificates. */
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_BYTE              CERT_VER [ DRM_VERSION_LEN ]          =
{
    0, 1, 0, 0
};

/* The version for client id. */
DRM_GLOBAL_CONST  DRM_DISCARDABLE DRM_BYTE              CLIENT_ID_VER [ DRM_VERSION_LEN ]     =
{
    2, 0, 0, 0
};
#endif

EXIT_PK_NAMESPACE_CODE;

