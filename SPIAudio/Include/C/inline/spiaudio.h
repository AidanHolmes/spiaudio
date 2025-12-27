#ifndef _INLINE_SPIAUDIO_H
#define _INLINE_SPIAUDIO_H

#ifndef CLIB_SPIAUDIO_PROTOS_H
#define CLIB_SPIAUDIO_PROTOS_H
#endif

#ifndef __INLINE_MACROS_H
#include <inline/macros.h>
#endif

#ifndef  EXEC_TYPES_H
#include <exec/types.h>
#endif

#ifndef SPIAUDIO_BASE_NAME
#define SPIAUDIO_BASE_NAME SPIAudioBase
#endif

#define MHIAllocDecoder(task, mhisignal) \
	LP2(0x1e, APTR, MHIAllocDecoder, struct Task *, task, a0, ULONG, mhisignal, d0, \
	, SPIAUDIO_BASE_NAME)

#define MHIFreeDecoder(handle) \
	LP1NR(0x24, MHIFreeDecoder, APTR, handle, a3, \
	, SPIAUDIO_BASE_NAME)

#define MHIQueueBuffer(handle, buffer, size) \
	LP3(0x2a, BOOL, MHIQueueBuffer, APTR, handle, a3, APTR, buffer, a0, ULONG, size, d0, \
	, SPIAUDIO_BASE_NAME)

#define MHIGetEmpty(handle) \
	LP1(0x30, APTR, MHIGetEmpty, APTR, handle, a3, \
	, SPIAUDIO_BASE_NAME)

#define MHIGetStatus(handle) \
	LP1(0x36, UBYTE, MHIGetStatus, APTR, handle, a3, \
	, SPIAUDIO_BASE_NAME)

#define MHIPlay(handle) \
	LP1NR(0x3c, MHIPlay, APTR, handle, a3, \
	, SPIAUDIO_BASE_NAME)

#define MHIStop(handle) \
	LP1NR(0x42, MHIStop, APTR, handle, a3, \
	, SPIAUDIO_BASE_NAME)

#define MHIPause(handle) \
	LP1NR(0x48, MHIPause, APTR, handle, a3, \
	, SPIAUDIO_BASE_NAME)

#define MHIQuery(query) \
	LP1(0x4e, ULONG, MHIQuery, ULONG, query, d1, \
	, SPIAUDIO_BASE_NAME)

#define MHISetParam(handle, param, value) \
	LP3NR(0x54, MHISetParam, APTR, handle, a3, ULONG, param, d0, ULONG, value, d1, \
	, SPIAUDIO_BASE_NAME)

#endif /*  _INLINE_SPIAUDIO_H  */
