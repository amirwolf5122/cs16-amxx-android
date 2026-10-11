#ifndef COMPAT_TYPES_H
#define COMPAT_TYPES_H

// Compatibility definitions expected by old metamod SDK headers,
// missing from hlsdk-portable. Types deliberately match the old
// HLSDK archtypes.h so typedef redefinition stays compatible.

#include <stddef.h>

typedef signed char        int8;
typedef unsigned char      uint8;
typedef short              int16;
typedef unsigned short     uint16;
typedef int                int32;
typedef unsigned int       uint32;
typedef long long          int64;
typedef unsigned long long uint64;

// From HLSDK engine/Sequence.h (dropped in hlsdk-portable)
typedef struct sequenceCommandLine_ sequenceCommandLine_s;
struct sequenceCommandLine_
{
	sequenceCommandLine_s* nextCommand;
	int                    commandType;
	int                    speakTime;
	char**                 ppSentenceText;
	char*                  sentenceName;
	int                    fileNameIndex;
	int                    entryIndex;
	int                    repeatCount;
	int                    waveIndex;
	int                    channel;
	int                    volume;
	int                    pitch;
	int                    attenuation;
};

typedef struct sequenceEntry_ sequenceEntry_s;
struct sequenceEntry_
{
	char*                   fileName;
	char*                   entryName;
	sequenceCommandLine_s*  firstCommand;
	sequenceEntry_s*        nextEntry;
	int                     isGlobal;
};

typedef struct sentenceEntry_ sentenceEntry_s;
struct sentenceEntry_
{
	char*              data;
	sentenceEntry_s*   nextEntry;
	int                isGlobal;
	unsigned int       index;
};

#endif

// hlsdk-portable renames this to SERVER_EXECUTE2 (winspool.h clash on
// Windows); AMXX core expects the classic metamod/hlsdk macro name.
#ifndef SERVER_EXECUTE
#define SERVER_EXECUTE  (*g_engfuncs.pfnServerExecute)
#endif
// ReGameDLL-era flag missing from hlsdk-portable cvardef.h; only used for
// the cosmetic flags-name string in CvarManager.
#ifndef FCVAR_NOEXTRAWHITEPACE
#define FCVAR_NOEXTRAWHITEPACE  (1<<9)
#endif
// ReGameDLL API headers reference CBaseEntity by pointer only; hlsdk-portable
// declares the class in cbase.h which resdk headers do not include.
#ifdef __cplusplus
class CBaseEntity;
#endif
