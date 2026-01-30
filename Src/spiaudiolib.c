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

#include "spiaudiolib.h"
#include "vs1053.h"
#include "debug.h"
#include <libraries/mhi.h>
#include <string.h>

#define _STR(A) #A
#define STR(A) _STR(A)

struct UserMHI{
	UWORD version;
	struct MsgPort *drvPort;
	struct Task *task;
	struct IOStdReq *std;
	struct VSParameterData *pdata;
	ULONG signal;
	UBYTE volume;
	UBYTE panning;
	UBYTE mixing;
	UBYTE bass;
	UBYTE treble;
	UBYTE mid;
	UBYTE midbass;
	UBYTE midhigh;
};

__INLINE__ void sendAndWait(struct VSData *dat, struct IOStdReq *std)
{
	PutMsg(dat->drvPort, (struct Message *)std);
	WaitPort(std->io_Message.mn_ReplyPort);
	while(GetMsg(std->io_Message.mn_ReplyPort));
}

__SAVE_DS__ struct LibDevBase* __ASM__ libdev_library_open(__REG__(a6, struct LibDevBase *)base)
{
	return base;
}

__SAVE_DS__ struct LibDevBase* __ASM__ libdev_initalise(__REG__(a6, struct LibDevBase *)base)
{
	struct VSData *config = NULL ;
	
	base->libData = config = (struct VSData *)initVS1053(5);
	if (!base->libData){
		return NULL;
	}

	return base;
}

__SAVE_DS__ void __ASM__ libdev_cleanup(__REG__(a6, struct LibDevBase *)base)
{
	struct VSData *dat = NULL ;
	if (base->libData){
		destroyVS1053((struct VSData*)base->libData); // This is done in worker task
		base->libData = NULL;
	}
}

///////////////////////////////////////////////////////////////
//
// Start of library functions
//
//
//

__SAVE_DS__ APTR __ASM__ MHIAllocDecoder (__REG__(a0, struct Task *)task , __REG__(d0, ULONG) mhisignal, __REG__(a6, struct LibDevBase *)base)
{
	BOOL completedOK = FALSE ;
	struct UserMHI *usr = NULL;
	struct VSData *dat = (struct VSData*)base->libData;
	
	if (!dat){
		goto end;
	}
	
	if (!(usr=AllocVec(sizeof(struct UserMHI), MEMF_ANY | MEMF_CLEAR))){
		goto end;
	}
	if (!(usr->std = AllocVec(sizeof(struct IOStdReq), MEMF_PUBLIC | MEMF_CLEAR))){
		goto end;
	}
	usr->version = LIBDEVMAJOR << 8 | LIBDEVMINOR;
	usr->drvPort = dat->drvPort;
	usr->task = task;
	usr->signal = mhisignal;

	if (!(usr->std->io_Message.mn_ReplyPort = CreateMsgPort())){
		goto end;
	}

	usr->std->io_Message.mn_Length = sizeof(struct IOStdReq) ;
	usr->std->io_Message.mn_Node.ln_Type = NT_MESSAGE;
	usr->std->io_Command = 0;

	if (!(usr->pdata = (struct VSParameterData*)AllocVec(sizeof(struct VSParameterData), MEMF_PUBLIC | MEMF_CLEAR))){
		goto end;
	}

	completedOK = TRUE ;
end:
	if (!completedOK && usr){
		if (usr->std){
			if (usr->std->io_Message.mn_ReplyPort ){
				DeleteMsgPort(usr->std->io_Message.mn_ReplyPort);
			}
			if (usr->pdata){
				FreeVec(usr->pdata);
			}
			FreeVec(usr->std);
		}
		FreeVec(usr);
		usr = NULL;
	}
	return usr;
}

__SAVE_DS__ VOID __ASM__ MHIFreeDecoder (__REG__(a3, APTR) handle, __REG__(a6, struct LibDevBase *)base)
{
	struct UserMHI *usr = (struct UserMHI*)handle;
	if (handle){
		DeleteMsgPort(usr->std->io_Message.mn_ReplyPort);
		FreeVec(usr->pdata);
		FreeVec(handle);
	}
}

__SAVE_DS__ BOOL __ASM__ MHIQueueBuffer (__REG__(a3, APTR) handle, __REG__(a0, APTR) buffer, __REG__(d0, ULONG) size, __REG__(a6, struct LibDevBase *)base)
{
	return addBuffer((struct VSData *)base->libData, buffer, size, ((struct UserMHI*)handle)->task, ((struct UserMHI*)handle)->signal);
}

__SAVE_DS__ APTR __ASM__ MHIGetEmpty (__REG__(a3, APTR) handle, __REG__(a6, struct LibDevBase *)base)
{
	struct VSChunk *buf = NULL;
	UBYTE *returnme = NULL ;
	struct VSData *dat = (struct VSData*)base->libData;
	
	ObtainSemaphore(&dat->sem);
	if ((buf = getUsedChunk(dat))){
		returnme = buf->buffer;
		removeChunk(buf);
	}
	ReleaseSemaphore(&dat->sem);
	
	if (returnme){
		return returnme;
	}
	return NULL;
}

__SAVE_DS__ UBYTE __ASM__ MHIGetStatus(__REG__(a3, APTR) handle, __REG__(a6, struct LibDevBase *)base)
{
	UBYTE status = MHIF_STOPPED;
	
	struct VSData *dat = (struct VSData*)base->libData;
	if (dat->status & VS_PLAYING | VS_PAUSED == VS_PLAYING){
		status = MHIF_PLAYING;
	}else{
		status = MHIF_PAUSED;
	}
	if (dat->status & VS_NOBUFF){
		status = MHIF_OUT_OF_DATA;
	}
	return status;
}

__SAVE_DS__ VOID __ASM__ MHIPlay(__REG__(a3, APTR) handle, __REG__(a6, struct LibDevBase *)base)
{
	struct UserMHI *usr = (struct UserMHI*)handle;
	struct VSData *dat = (struct VSData*)base->libData;
	
	D(DebugPrint(DEBUG_LEVEL, "MHIPlay: called\n"));
	usr->std->io_Command = CMD_START;
	sendAndWait(dat, usr->std);
}

__SAVE_DS__ VOID __ASM__ MHIStop(__REG__(a3, APTR) handle, __REG__(a6, struct LibDevBase *)base)
{
	struct Node *n = NULL;
	struct VSData *dat = (struct VSData*)base->libData;
	struct UserMHI *usr = (struct UserMHI*)handle;
	struct VSChunk *buf = NULL ;
	
	D(DebugPrint(DEBUG_LEVEL, "MHIStop: called\n"));
	
	usr->std->io_Command = CMD_RESET;
	sendAndWait(dat, usr->std);

	ObtainSemaphore(&dat->sem);
	removeAllChunks(dat);
	ReleaseSemaphore(&dat->sem);
}

__SAVE_DS__ VOID __ASM__ MHIPause(__REG__(a3, APTR) handle, __REG__(a6, struct LibDevBase *)base)
{
	UBYTE status = MHIF_STOPPED;
	struct UserMHI *usr = (struct UserMHI*)handle;
	struct VSData *dat = (struct VSData*)base->libData;
	
	D(DebugPrint(DEBUG_LEVEL, "MHIPause: called\n"));
	
	status = MHIGetStatus(handle, base);
	
	if (status == MHIF_PAUSED){
		usr->std->io_Command = CMD_START; // resume from paused
	}else{
		usr->std->io_Command = CMD_STOP; // stop/pause playback
	}
	sendAndWait(dat, usr->std);
}

__SAVE_DS__ ULONG __ASM__ MHIQuery(__REG__(d1, ULONG) query, __REG__(a6, struct LibDevBase *)base)
{
		switch (query) {
		// Supported encoding
		case MHIQ_MPEG1: 
		case MHIQ_MPEG2: 
		case MHIQ_MPEG25: 
		case MHIQ_MPEG4:
			return MHIF_SUPPORTED;
			
		// Supported layers
		case MHIQ_LAYER1:
		case MHIQ_LAYER2:
#if VS1053_AUDIO_MPEG12
			return MHIF_SUPPORTED;
#else
			return MHIF_UNSUPPORTED;
#endif
			
		case MHIQ_LAYER3:
			return MHIF_SUPPORTED;
			
		case MHIQ_CAPABILITIES:
			return (ULONG)"audio/mpeg{audio/mp2,audio/mp3},audio/ogg{audio/vorbis,audio/opus},audio/mp4{audio/aac},audio/aac,audio/flac,audio/wav";
			
		// Supported features
		case MHIQ_VARIABLE_BITRATE:
		case MHIQ_JOINT_STEREO:
		case MHIQ_BASS_CONTROL: 
		case MHIQ_TREBLE_CONTROL:
		case MHIQ_VOLUME_CONTROL: 
		case MHIQ_PANNING_CONTROL:
		case MHIQ_CROSSMIXING:
		case MHIQ_PREFACTOR_CONTROL: 	// let's pretend this is supported		
		case MHIQ_MID_CONTROL: 			// let's pretend this is supported (it is in VS1063)
		case MHIQ_5_BAND_EQ:
			return MHIF_SUPPORTED;
			
		case MHIQ_10_BAND_EQ:
			return MHIF_UNSUPPORTED;
			
		// Other query values
		case MHIQ_IS_HARDWARE:
			return MHIF_TRUE;

		case MHIQ_IS_68K: 
		case MHIQ_IS_PPC:
			return MHIF_FALSE;
			
		case MHIQ_DECODER_NAME:
			return (ULONG)"SPIder VS1053/1063";

		case MHIQ_DECODER_VERSION:
			return (ULONG)STR(LIBDEVMAJOR) "." STR(LIBDEVMINOR); 
			
		case MHIQ_AUTHOR:
			return (ULONG)"Aidan Holmes";
			
		default:
			return MHIF_UNSUPPORTED;
	}
}

__SAVE_DS__ VOID __ASM__ MHISetParam(__REG__(a3, APTR) handle, __REG__(d0, UWORD) param, __REG__(d1, ULONG) value, __REG__(a6, struct LibDevBase *)base)
{
	struct UserMHI *usr = (struct UserMHI*)handle;
	struct VSData *dat = (struct VSData*)base->libData;
	
	usr->std->io_Command = CMD_VSAUDIO_PARAMETER;
	usr->pdata->value = value ;
	usr->std->io_Length = sizeof(struct VSParameterData);
	usr->std->io_Data = usr->pdata; // needs to be public memory to share with driver
	D(DebugPrint(DEBUG_LEVEL, "MHISetParam: param %u, value %u\n", param, value));
	switch (param) {
		case MHIP_VOLUME:
			if(value != usr->volume) {
				usr->pdata->parameter = VS_PARAM_VOL;
				sendAndWait(dat, usr->std);
				usr->volume = usr->pdata->actual;
			}
			break;
	
		case MHIP_PANNING:
			if(value != usr->panning) {
				usr->pdata->parameter = VS_PARAM_PAN;
				sendAndWait(dat, usr->std);
				usr->panning = usr->pdata->actual;
			}
			break;

		case MHIP_CROSSMIXING:
			if(value != usr->mixing) {
				usr->pdata->parameter = VS_PARAM_CROSSMIX;
				sendAndWait(dat, usr->std);
				usr->mixing = usr->pdata->actual;
			}
			break;

		case MHIP_BASS:
			if(value != usr->bass) {
				usr->pdata->parameter = VS_PARAM_BASS;
				sendAndWait(dat, usr->std);
				usr->bass = usr->pdata->actual;
			}
			break;

		case MHIP_TREBLE:
			if(value != usr->treble) {
				usr->pdata->parameter = VS_PARAM_TREBLE;
				sendAndWait(dat, usr->std);
				usr->treble = usr->pdata->actual;
			}
			break;

		case MHIP_MID:
			if(value != usr->treble) {
				usr->pdata->parameter = VS_PARAM_MID;
				sendAndWait(dat, usr->std);
				usr->mid = usr->pdata->actual;
			}
			break;
			
		case MHIP_MIDBASS:
			if(value != usr->treble) {
				usr->pdata->parameter = VS_PARAM_MIDBASS;
				sendAndWait(dat, usr->std);
				usr->midbass = usr->pdata->actual;
			}
			break;
			
		case MHIP_MIDHIGH:
			if(value != usr->treble) {
				usr->pdata->parameter = VS_PARAM_MIDHIGH;
				sendAndWait(dat, usr->std);
				usr->midhigh = usr->pdata->actual;
			}
			break;
			
		default:
			break;
	}
}


