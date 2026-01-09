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

#include <stdio.h>
#include "vs1053.h"
#include "spi.h"
#include <exec/types.h>
#include <exec/resident.h>
#include <exec/exec.h>
#include <proto/exec.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <exec/io.h>
#include <string.h>

int  __regargs _CXBRK(void) { return 0; } /* Disable SAS/C Ctrl-C handling */
void __regargs __chkabort(void) { ; } /* really */

int main(int argc, char **argv)
{
	ULONG sigs = 0, i=0 ;
	LONG length;
	struct VSData *vsdat = NULL;
	char *musicFileName;
	BPTR f = NULL;
	const int chunksAvailable = 5, bufSize = 1024;
	UBYTE *allMem = NULL, *buffer = NULL;
	BYTE mySig = -1 ;
	struct VSChunk *chunk = NULL;
	struct IOStdReq *std = NULL ;
	
	if (argc >= 2){
		musicFileName = argv[1];
	}else{
		printf("No arguments found. Provide name of music file\n") ;
		return 1 ;
	}
	
	if ((mySig = AllocSignal(-1)) < 0){
		printf("Couldn't assign signal\n");
		return 1;
	}
	if (!(f=Open(musicFileName, MODE_OLDFILE))){
		printf("Couldn't open file %s\n", musicFileName);
		goto exit;
	}
	
	if (!(std = AllocVec(sizeof(struct IOStdReq), MEMF_PUBLIC | MEMF_CLEAR))){
		printf("Cannot allocate required memory for IO\n");
		goto exit;
	}
	
	if (!(vsdat=initVS1053(0))){
		printf("Failed to initialise VS1053\n");
		goto exit;
	}
	
	allMem = AllocVec(bufSize * chunksAvailable, MEMF_ANY);
	if (!allMem){
		goto exit;
	}
	if ((length= Read(f, allMem, bufSize * chunksAvailable))<0){
		printf("Read failure, returned %d\n", length);
	}
	for(i=0; i<length;i+=bufSize){
		if (!addBuffer(vsdat, allMem + i, (length - (i+bufSize)) > bufSize? bufSize:length - (i+bufSize), FindTask(NULL), 1 << mySig)){
			printf("Cannot add new buffer\n");
			goto exit;
		}
	}
	
	if (!(std->io_Message.mn_ReplyPort = CreateMsgPort())){
		goto exit;
	}else{
		std->io_Message.mn_Length = sizeof(struct IOStdReq) ;
		std->io_Message.mn_Node.ln_Type = NT_MESSAGE;
		std->io_Command = 0;
	}
	
	std->io_Command = CMD_START; // start playback
	PutMsg(vsdat->drvPort, (struct Message *)std);
	WaitPort(std->io_Message.mn_ReplyPort);	
	GetMsg(std->io_Message.mn_ReplyPort); //while(GetMsg(std->io_Message.mn_ReplyPort));
	
	printf("Note that running this test app could break any MHI instance also running\nPress CTRL-C to pause\n");
	for ( ; ; ){
		sigs = Wait (SIGBREAKF_CTRL_C | (1 << mySig));
		if (sigs & SIGBREAKF_CTRL_C){
			printf("Pausing - press CTRL-C again to stop\n");
			std->io_Command = CMD_STOP; // stop/pause playback
			PutMsg(vsdat->drvPort, (struct Message *)std);
			WaitPort(std->io_Message.mn_ReplyPort);	
			while(GetMsg(std->io_Message.mn_ReplyPort));
			break;
		}else{ // mySig triggered meaning new buffer or status change
			//printf("%");
			ObtainSemaphore(&vsdat->sem);
			while (chunk=getUsedChunk(vsdat)){
				resetChunk(chunk); // Just reuse existing chunk????
				buffer = chunk->buffer; 
				//removeChunk(chunk);
				if ((length = Read(f, buffer, bufSize))<0){
					printf("Read failure, returned %d\n", length);
					ReleaseSemaphore(&vsdat->sem);
					goto exit;
				}else{
					if (length > 0){
						//printf("&");
						updateChunk(vsdat, chunk, length, FindTask(NULL), 1 << mySig);
						//if (!addBuffer(vsdat, buffer, length, FindTask(NULL), 1 << mySig)){
						//	printf("Cannot add new buffer\n");
						//	goto exit;
						//}
					}
				}
			}
			ReleaseSemaphore(&vsdat->sem);
		}

	}
	
	for ( ; ; ){
		sigs = Wait (SIGBREAKF_CTRL_C);
		if (sigs & SIGBREAKF_CTRL_C){
			printf("Stopping...\n");
			std->io_Command = CMD_RESET; // stop playback
			PutMsg(vsdat->drvPort, (struct Message *)std);
			WaitPort(std->io_Message.mn_ReplyPort);
			while(GetMsg(std->io_Message.mn_ReplyPort));
			printf("Stopped\n");
			break;
		}

	}
	
exit:
	if (allMem){
		FreeVec(allMem);
	}
	Close(f);
	if (std->io_Message.mn_ReplyPort){
		DeleteMsgPort(std->io_Message.mn_ReplyPort);
	}
	if (std){
		FreeVec(std);
	}
	FreeSignal(mySig);
	destroyVS1053(vsdat);
	
	return 0;
}