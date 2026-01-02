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

struct UserMHI{
	struct Task *task;
	struct IOStdReq std;
	ULONG signal;
	UBYTE status;
	UBYTE laststatus;
	UBYTE volume;
	UBYTE panning;
	UBYTE mixing;
	UBYTE prefactor;
	UBYTE bass;
	UBYTE treble;
	UBYTE mid;
};


struct LibDevBase* __saveds __asm libdev_library_open(register __a6 struct LibDevBase *base)
{
	return base;
}

struct LibDevBase* __saveds __asm libdev_initalise(register __a6 struct LibDevBase *base)
{
	struct VSData *config = NULL ;
	
	base->libData = config = (struct VSData *)initVS1053();
	if (!base->libData){
		return NULL;
	}

	return base;
}

void __saveds __asm libdev_cleanup(register __a6 struct LibDevBase *base)
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

APTR    __saveds __asm MHIAllocDecoder (register __a0 struct Task *task , register __d0 ULONG mhisignal, register __a6 struct LibDevBase *base)
{
	struct UserMHI *usr = NULL;
	if ((usr=AllocVec(sizeof(struct UserMHI), MEMF_ANY | MEMF_CLEAR))){
		usr->task = task;
		usr->signal = mhisignal;
		memset(&usr->std, 0, sizeof(struct IOStdReq));
		if (!(usr->std.io_Message.mn_ReplyPort = CreateMsgPort())){
			FreeVec(usr);
			usr = NULL ;
		}else{
			usr->std.io_Message.mn_Length = sizeof(struct IOStdReq) ;
			usr->std.io_Message.mn_Node.ln_Type = NT_MESSAGE;
			usr->std.io_Command = 0;
		}
	}
	return usr;
}

VOID    __saveds __asm MHIFreeDecoder  (register __a3 APTR handle, register __a6 struct LibDevBase *base)
{
	struct UserMHI *usr = (struct UserMHI*)handle;
	if (handle){
		DeleteMsgPort(usr->std.io_Message.mn_ReplyPort);
		FreeVec(handle);
	}
}

BOOL    __saveds __asm MHIQueueBuffer  (register __a3 APTR handle, register __a0 APTR buffer, register __d0 ULONG size, register __a6 struct LibDevBase *base)
{
	return addBuffer((struct VSData *)base->libData, buffer, size, ((struct UserMHI*)handle)->task, ((struct UserMHI*)handle)->signal);
}

APTR    __saveds __asm MHIGetEmpty     (register __a3 APTR handle, register __a6 struct LibDevBase *base)
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

UBYTE   __saveds __asm MHIGetStatus    (register __a3 APTR handle, register __a6 struct LibDevBase *base)
{
	struct VSData *dat = (struct VSData*)base->libData;
	if (dat->status & VS_NOBUFF){
		((struct UserMHI*)handle)->status = MHIF_OUT_OF_DATA;
	}
	return ((struct UserMHI*)handle)->status;
}

VOID    __saveds __asm MHIPlay         (register __a3 APTR handle, register __a6 struct LibDevBase *base)
{
	struct UserMHI *usr = (struct UserMHI*)handle;
	struct VSData *dat = (struct VSData*)base->libData;
	
	usr->std.io_Command = CMD_START;
	PutMsg(dat->drvPort, (struct Message *)&usr->std);
	WaitPort(usr->std.io_Message.mn_ReplyPort);	
	
	((struct UserMHI*)handle)->status = MHIF_PLAYING;
}

VOID    __saveds __asm MHIStop         (register __a3 APTR handle, register __a6 struct LibDevBase *base)
{
	struct Node *n = NULL;
	struct VSData *dat = (struct VSData*)base->libData;
	struct UserMHI *usr = (struct UserMHI*)handle;
	struct VSChunk *buf = NULL ;
	
	usr->std.io_Command = CMD_RESET;
	PutMsg(dat->drvPort, (struct Message *)&usr->std);
	WaitPort(usr->std.io_Message.mn_ReplyPort);	

	((struct UserMHI*)handle)->status = MHIF_STOPPED;
	ObtainSemaphore(&dat->sem);
	removeAllChunks(dat);
	ReleaseSemaphore(&dat->sem);
}

VOID    __saveds __asm MHIPause        (register __a3 APTR handle, register __a6 struct LibDevBase *base)
{
	struct UserMHI *usr = (struct UserMHI*)handle;
	struct VSData *dat = (struct VSData*)base->libData;
	
	if (((struct UserMHI*)handle)->status == MHIF_PAUSED){
		usr->std.io_Command = CMD_START; // resume from paused
	}else{
		usr->std.io_Command = CMD_STOP; // stop/pause playback
	}
	PutMsg(dat->drvPort, (struct Message *)&usr->std);
	WaitPort(usr->std.io_Message.mn_ReplyPort);	
	
	if(((struct UserMHI*)handle)->status == MHIF_PAUSED){
		((struct UserMHI*)handle)->status = MHIF_PLAYING;
	}else{
		if(((struct UserMHI*)handle)->status == MHIF_PLAYING){
			((struct UserMHI*)handle)->status = MHIF_PAUSED;
		}
	}
}

ULONG   __saveds __asm MHIQuery        (register __d1 ULONG query, register __a6 struct LibDevBase *base)
{
		switch (query) {
		case MHIQ_MPEG1: case MHIQ_MPEG2: case MHIQ_MPEG25:
			return MHIF_SUPPORTED;
			break;
		
		case MHIQ_MPEG4:
			return MHIF_UNSUPPORTED;
			break;
			
		case MHIQ_LAYER1:
			return MHIF_UNSUPPORTED;
			break;
			
		case MHIQ_LAYER2: case MHIQ_LAYER3:
			return MHIF_SUPPORTED;
			break;
			
		case MHIQ_VARIABLE_BITRATE:
			return MHIF_UNSUPPORTED;
			break;
		
		//case MHIQ_JOINT_STERIO:
		//	return MHIF_SUPPORTED;
		//	break;
			
		case MHIQ_BASS_CONTROL: case MHIQ_TREBLE_CONTROL: case MHIQ_PREFACTOR_CONTROL:
			return MHIF_SUPPORTED;
			break;
			
		case MHIQ_MID_CONTROL:
			return MHIF_UNSUPPORTED;
			
		case MHIQ_VOLUME_CONTROL: case MHIQ_PANNING_CONTROL:
			return MHIF_SUPPORTED;
			break;

		case MHIQ_CROSSMIXING:
			return MHIF_UNSUPPORTED;
			
		case MHIQ_IS_HARDWARE:
			return MHIF_TRUE;
			break;

		case MHIQ_IS_68K: case MHIQ_IS_PPC:
			return MHIF_FALSE;
			break;
			
		case MHIQ_DECODER_NAME:
			return (ULONG)"Spider Audio";
			break;

		case MHIQ_DECODER_VERSION:
			return (ULONG)"1";
			break;
			
		case MHIQ_AUTHOR:
			return (ULONG)"Paul Qureshi, Thomas Wenzel";
			break;
			
		default:
			return MHIF_UNSUPPORTED;
			break;			
	}
}

VOID    __saveds __asm MHISetParam     (register __a3 APTR handle, register __d0 UWORD param, register __d1 ULONG value, register __a6 struct LibDevBase *base)
{
	LONG calc;
	UBYTE r;
	UBYTE l;

	switch (param) {
		case MHIP_VOLUME:
			if(value != ((struct UserMHI*)handle)->volume) {

			}

			break;
		
		case MHIP_PANNING:
			if(value != ((struct UserMHI*)handle)->panning) {

			}
			break;

		case MHIP_CROSSMIXING:
			if(value != ((struct UserMHI*)handle)->mixing) {
				((struct UserMHI*)handle)->mixing = value;
			}
			break;
			
		case MHIP_PREFACTOR:
			if(value != ((struct UserMHI*)handle)->prefactor) {
				((struct UserMHI*)handle)->prefactor = value;
			}
			break;

		case MHIP_BASS:
			if(value != ((struct UserMHI*)handle)->bass) {
				((struct UserMHI*)handle)->bass = value;

			}
			break;

		case MHIP_TREBLE:
			if(value != ((struct UserMHI*)handle)->treble) {
				((struct UserMHI*)handle)->treble = value;

			}
			break;
	}
}


