#ifndef _INCLUDE_PRAGMA_SPIAUDIO_LIB_H
#define _INCLUDE_PRAGMA_SPIAUDIO_LIB_H

#ifndef CLIB_SPIAUDIO_PROTOS_H
#include <clib/spiaudio_protos.h>
#endif

#if defined(AZTEC_C) || defined(__MAXON__) || defined(__STORM__)
#pragma amicall(SPIAudioBase,0x01e,MHIAllocDecoder(a0,d0))
#pragma amicall(SPIAudioBase,0x024,MHIFreeDecoder(a3))
#pragma amicall(SPIAudioBase,0x02a,MHIQueueBuffer(a3,a0,d0))
#pragma amicall(SPIAudioBase,0x030,MHIGetEmpty(a3))
#pragma amicall(SPIAudioBase,0x036,MHIGetStatus(a3))
#pragma amicall(SPIAudioBase,0x03c,MHIPlay(a3))
#pragma amicall(SPIAudioBase,0x042,MHIStop(a3))
#pragma amicall(SPIAudioBase,0x048,MHIPause(a3))
#pragma amicall(SPIAudioBase,0x04e,MHIQuery(d1))
#pragma amicall(SPIAudioBase,0x054,MHISetParam(a3,d0,d1))
#endif
#if defined(_DCC) || defined(__SASC)
#pragma  libcall SPIAudioBase MHIAllocDecoder        01e 0802
#pragma  libcall SPIAudioBase MHIFreeDecoder         024 b01
#pragma  libcall SPIAudioBase MHIQueueBuffer         02a 08b03
#pragma  libcall SPIAudioBase MHIGetEmpty            030 b01
#pragma  libcall SPIAudioBase MHIGetStatus           036 b01
#pragma  libcall SPIAudioBase MHIPlay                03c b01
#pragma  libcall SPIAudioBase MHIStop                042 b01
#pragma  libcall SPIAudioBase MHIPause               048 b01
#pragma  libcall SPIAudioBase MHIQuery               04e 101
#pragma  libcall SPIAudioBase MHISetParam            054 10b03
#endif

#endif	/*  _INCLUDE_PRAGMA_SPIAUDIO_LIB_H  */
