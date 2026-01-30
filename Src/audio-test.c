/* Copyright 2026 Aidan Holmes

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
#include <exec/types.h>
#include <exec/resident.h>
#include <exec/exec.h>
#include <proto/exec.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <exec/io.h>
#include <string.h>
#include <proto/mhi.h>
#include <libraries/mhi.h>
#ifdef __VBCC__
#include <inline/mhi_protos.h>
#endif

// SAS/C using compiler instruction to stop CTRL-C handling. These conflict so commented out
//int  __regargs _CXBRK(void) { return 0; } /* Disable SAS/C Ctrl-C handling */
//void __regargs __chkabort(void) { ; } /* really */

#ifdef __VBCC__
void __chkabort(void) { ; }
#endif

int main(int argc, char **argv)
{
	const int chunksAvailable = 5, bufSize = 1024;
	struct Library *MHIBase = NULL, *DOSBase = NULL ;
	ULONG sigs = 0, i=0 ;
	LONG length;
	char *musicFileName;
	BPTR f = 0;
	UBYTE *allMem = NULL, *buffer = NULL;
	BYTE mySig = -1 ;
	APTR handle = NULL ;
	
	//SetTaskPri(FindTask(0), 10);
	if (!(MHIBase = OpenLibrary("LIBS:MHI/mhispiaudio.library", 0))){
		printf("Cannot open LIBS:MHI/mhispiaudio.library\n");
		goto exit;
	}
	
	if (!(DOSBase = OpenLibrary("dos.library", 0))){
		printf("Cannot open LIBS:MHI/mhispiaudio.library\n");
		goto exit;
	}
	
	if (argc >= 2){
		musicFileName = argv[1];
	}else{
		printf("No arguments found. Provide name of music file\n") ;
		goto exit;
	}
	
	if ((mySig = AllocSignal(-1)) < 0){
		printf("Couldn't assign signal\n");
		goto exit;
	}
	
	if (!(handle = MHIAllocDecoder (FindTask(NULL) , 1 << mySig))){
		printf("Couldn't alloc MHI handle\n") ;
		goto exit;
	}
	
	if (!(f=Open(musicFileName, MODE_OLDFILE))){
		printf("Couldn't open file %s\n", musicFileName);
		goto exit;
	}
	
	allMem = AllocVec(bufSize * chunksAvailable, MEMF_ANY);
	if (!allMem){
		goto exit;
	}
	if ((length= Read(f, allMem, bufSize * chunksAvailable))<0){
		printf("%s read failure, returned %d\n", musicFileName, length);
	}
	for(i=0; i<length;i+=bufSize){
		if (!(MHIQueueBuffer(handle, allMem + i, ((length - (i+bufSize)) > bufSize)? bufSize:(length - (i+bufSize))))){
			printf("Error: cannot add a new buffer\n");
			goto exit;
		}
	}
	MHISetParam(handle, MHIP_VOLUME, 100);
	MHIPlay(handle);
	printf("Playing, waiting on sig bit %d\n", mySig);
	
	for ( ; ; ){
		sigs = Wait (SIGBREAKF_CTRL_C | (1 << mySig));
		if (sigs & SIGBREAKF_CTRL_C){
			printf("Stopping - press CTRL-C again to exit\n");
			break;
		}else{ // mySig triggered meaning new buffer or status change
			if ((buffer=MHIGetEmpty(handle))){
				if ((length = Read(f, buffer, bufSize))<0){
					printf("%s read failure, returned error %d\n", musicFileName, length);
					goto exit;
				}else{
					if (length == 0){
						// no more file to read
						printf("Finished playing %s, press CTRL-C again to exit\n", musicFileName);
						break;
					}
					if (!(MHIQueueBuffer(handle, buffer, length))){
						printf("Error: cannot add a new buffer\n");
						goto exit;
					}
				}
			}
		}
	}
	
	MHIStop(handle);
	
	for ( ; ; ){
		sigs = Wait (SIGBREAKF_CTRL_C);
		if (sigs & SIGBREAKF_CTRL_C){
			printf("Exiting...\n");
			break;
		}

	}
	
exit:
	if (handle){
		MHIFreeDecoder(handle);
	}
	if (allMem){
		FreeVec(allMem);
	}
	Close(f);
	FreeSignal(mySig);
	if (DOSBase){
		CloseLibrary(DOSBase);
	}
	if (MHIBase){
		CloseLibrary(MHIBase);
	}
	
	return 0;
}