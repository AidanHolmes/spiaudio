#ifndef _PROTO_SPIAUDIO_H
#define _PROTO_SPIAUDIO_H

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif
#if !defined(CLIB_SPIAUDIO_PROTOS_H) && !defined(__GNUC__)
#include <clib/spiaudio_protos.h>
#endif

#ifndef __NOLIBBASE__
extern struct Library *SPIAudioBase;
#endif

#ifdef __GNUC__
#include <inline/spiaudio.h>
#elif defined(__VBCC__)
#if defined(__MORPHOS__) || !defined(__PPC__)
#include <inline/spiaudio_protos.h>
#endif
#else
#include <pragma/spiaudio_lib.h>
#endif

#endif	/*  _PROTO_SPIAUDIO_H  */
