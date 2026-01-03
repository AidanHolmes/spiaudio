/* Automatically generated header! Do not edit! */

#ifndef CLIB_SPIAUDIO_PROTOS_H
#define CLIB_SPIAUDIO_PROTOS_H


/*
**	$VER: spiaudio_protos.h 1.0 (03.01.2026)
**
**	C prototypes. For use with 32 bit integers only.
**
**	Copyright (C) 2026 Aidan Holmes
**	All Rights Reserved
*/

#ifndef  EXEC_TYPES_H
#include <exec/types.h>
#endif

APTR MHIAllocDecoder(struct Task * task, ULONG mhisignal);
VOID MHIFreeDecoder(APTR handle);
BOOL MHIQueueBuffer(APTR handle, APTR buffer, ULONG size);
APTR MHIGetEmpty(APTR handle);
UBYTE MHIGetStatus(APTR handle);
VOID MHIPlay(APTR handle);
VOID MHIStop(APTR handle);
VOID MHIPause(APTR handle);
ULONG MHIQuery(ULONG query);
VOID MHISetParam(APTR handle, ULONG param, ULONG value);

#endif	/*  CLIB_SPIAUDIO_PROTOS_H  */
