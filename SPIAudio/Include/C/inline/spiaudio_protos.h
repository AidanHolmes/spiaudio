#ifndef _VBCCINLINE_SPIAUDIO_H
#define _VBCCINLINE_SPIAUDIO_H

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif

APTR __MHIAllocDecoder(__reg("a6") struct Library *, __reg("a0") struct Task * task, __reg("d0") ULONG mhisignal)="\tjsr\t-30(a6)";
#define MHIAllocDecoder(task, mhisignal) __MHIAllocDecoder(SPIAudioBase, (task), (mhisignal))

VOID __MHIFreeDecoder(__reg("a6") struct Library *, __reg("a3") APTR handle)="\tjsr\t-36(a6)";
#define MHIFreeDecoder(handle) __MHIFreeDecoder(SPIAudioBase, (handle))

BOOL __MHIQueueBuffer(__reg("a6") struct Library *, __reg("a3") APTR handle, __reg("a0") APTR buffer, __reg("d0") ULONG size)="\tjsr\t-42(a6)";
#define MHIQueueBuffer(handle, buffer, size) __MHIQueueBuffer(SPIAudioBase, (handle), (buffer), (size))

APTR __MHIGetEmpty(__reg("a6") struct Library *, __reg("a3") APTR handle)="\tjsr\t-48(a6)";
#define MHIGetEmpty(handle) __MHIGetEmpty(SPIAudioBase, (handle))

UBYTE __MHIGetStatus(__reg("a6") struct Library *, __reg("a3") APTR handle)="\tjsr\t-54(a6)";
#define MHIGetStatus(handle) __MHIGetStatus(SPIAudioBase, (handle))

VOID __MHIPlay(__reg("a6") struct Library *, __reg("a3") APTR handle)="\tjsr\t-60(a6)";
#define MHIPlay(handle) __MHIPlay(SPIAudioBase, (handle))

VOID __MHIStop(__reg("a6") struct Library *, __reg("a3") APTR handle)="\tjsr\t-66(a6)";
#define MHIStop(handle) __MHIStop(SPIAudioBase, (handle))

VOID __MHIPause(__reg("a6") struct Library *, __reg("a3") APTR handle)="\tjsr\t-72(a6)";
#define MHIPause(handle) __MHIPause(SPIAudioBase, (handle))

ULONG __MHIQuery(__reg("a6") struct Library *, __reg("d1") ULONG query)="\tjsr\t-78(a6)";
#define MHIQuery(query) __MHIQuery(SPIAudioBase, (query))

VOID __MHISetParam(__reg("a6") struct Library *, __reg("a3") APTR handle, __reg("d0") ULONG param, __reg("d1") ULONG value)="\tjsr\t-84(a6)";
#define MHISetParam(handle, param, value) __MHISetParam(SPIAudioBase, (handle), (param), (value))

#endif /*  _VBCCINLINE_SPIAUDIO_H  */
