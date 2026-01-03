/* Copyright 2025 Aidan Holmes

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

# http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
*/

#ifndef __H_VS1053_AUDIO
#define __H_VS1053_AUDIO

#include <stdio.h>
#include "spi.h"
#include "timing.h"
#include "config_file.h"
#include <exec/semaphores.h>
#include <dos/dos.h>
#include <proto/dos.h>

// Status flags
#define VS_NEWSTART		0x0001
#define VS_PLAYING 		0x0002
#define VS_PAUSED 		0x0004
#define VS_STOPPING		0x0008
#define VS_NOBUFF		0x0010

// Parameter values
#define VS_PARAM_VOL		0
#define VS_PARAM_PAN		1
#define VS_PARAM_CROSSMIX	2
#define VS_PARAM_BASS		3
#define VS_PARAM_TREBLE		4

// Additional commands
#define CMD_VSAUDIO_PARAMETER		CMD_NONSTD + 0


struct VSParameterData
{
	UWORD parameter;	// Parameter to change
	ULONG value;	// Value requested
	ULONG actual;	// Actual value used (return)
};

// This can be overridden at compile time
// to allow or remove MPEG layer 1 and 2 decoding
#ifndef VS1053_AUDIO_MPEG12
#define VS1053_AUDIO_MPEG12 1
#endif

struct VSChunk
{
	struct Node _n;
	UBYTE *buffer;
	ULONG size;
	ULONG offset;
	ULONG sigs;		// Signal to communicate back to original buffer owner
	struct Task *ownerTask;
};

struct VSData
{
	struct IORequest *tmr;
	struct Task *drvTask;
	struct Task *callingTask; // temp for start up
	struct MsgPort *drvPort;
	struct SignalSemaphore sem;
	struct VSChunk *currentChunk;
	struct ClockportConfig config;
	BYTE callingSig; // temp for start up
	BYTE sig;
	BYTE sigTerm;
	BOOL initspi;
	UBYTE endfill ;
	ULONG iterWait;
	struct List bufferList;
	struct List freeList;
	UWORD status;
	UBYTE panning; // 0 full left 50 centre, 100 full right
	UBYTE volume; // 0 to 100%
	UBYTE crossmixing; // 0 to 100
	UBYTE bass;
	UBYTE treb;
};


struct VSData* initVS1053(void);
BOOL resetVS1053(struct VSData *dat);
void destroyVS1053(struct VSData *dat);

BOOL playChunk(struct VSData *dat);

BOOL addBuffer(struct VSData *dat, APTR buffer, ULONG size, struct Task *task, ULONG sigs);

BOOL stopPlayback(struct VSData *dat);
void pausePlayback(struct VSData *dat, BOOL pause);

// Get empty/used chunk from the buffer list. This returns the VSChunk but doesn't remove from list
// Call removeChunk to release chunk or resetChunk and update buffer contents and size to reuse.
struct VSChunk* getUsedChunk(struct VSData *dat);
void resetChunk(struct VSChunk *chunk);
void removeChunk(struct VSChunk *chunk);
void removeAllChunks(struct VSData *dat);
void updateChunk(struct VSData *dat, struct VSChunk *chunk, ULONG size, struct Task *task, ULONG sigs);















#endif