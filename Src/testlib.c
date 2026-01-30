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

#include <proto/mhi.h>
#include <exec/types.h>
#include <exec/exec.h>
#include <proto/exec.h>
#include <stdio.h>
#include <string.h>
#include "timing.h"
#include "vs1053.h"
#ifdef __VBCC__
#include <inline/mhi_protos.h>
#endif

// SAS/C using compiler instruction to stop CTRL-C handling. These conflict so commented out
//int  __regargs _CXBRK(void) { return 0; } /* Disable SAS/C Ctrl-C handling */
//void __regargs __chkabort(void) { ; } /* really */


#ifdef __VBCC__
void __chkabort(void) { ; }
#endif

struct MinHandle{
	UWORD version;
	struct MsgPort *port;
};

static void waitStd(struct IOStdReq *std, struct MsgPort *port)
{
	PutMsg(port, (struct Message *)std);
	WaitPort(std->io_Message.mn_ReplyPort);	
	while(GetMsg(std->io_Message.mn_ReplyPort));
}

static void doVUMeter(struct IOStdReq *std, struct MsgPort *port)
{
	struct VSParameterData param;
	
	param.parameter = VS_PARAM_VUMETER;
	param.value = 0;
	std->io_Command = CMD_VSAUDIO_PARAMETER; // 
	std->io_Length = sizeof(struct VSParameterData);
	std->io_Data = &param;
	waitStd(std, port);
	printf("Left %udb, Right %udb\n",param.actual >> 8, param.actual & 0x00FF);
}

static void doInfo(struct IOStdReq *std, struct MsgPort *port)
{
	struct VSMediaInfo info;
	
	std->io_Command = CMD_VSAUDIO_INFO; // 
	std->io_Length = sizeof(struct VSMediaInfo);
	std->io_Data = &info;
	waitStd(std, port);
	printf("Hardware attached is %s\n", info.hwVersion==4?"VS1053":"VS1063");
	printf("HDAT0 0x%04X, HDAT1 0x%04X\n", info.hdat0, info.hdat1);
	printf("Playing %s at bitrate %dHz\n", info.audata&0x0001?"stereo":"mono", (info.audata >> 1)*2);
	printf("OGG/WAV position at %dMsec\n", info.positionMsec);
}

int main (int argc, char **argv)
{
	struct Library *MHIBase = NULL ;
	APTR handle = NULL ;
	BYTE mySig = -1;
	struct IORequest *tmr = NULL;
	ULONG sigmask = 0, sigs = 0;
	struct MinHandle *drv;
	UWORD buildVersion = LIBDEVMAJOR << 8 | LIBDEVMINOR;
	struct IOStdReq std ;
	
	memset(&std, 0, sizeof(struct IOStdReq));
	
	if (!(MHIBase = OpenLibrary("LIBS:MHI/mhispiaudio.library", 0))){
		printf("Cannot open MHI library\n");
		return 0;
	}
	
	if ((mySig = AllocSignal(-1)) < 0){
		printf("Couldn't alloc signal\n") ;
		goto exit;
	}

	if (!(handle = MHIAllocDecoder (FindTask(NULL) , mySig))){
		printf("Couldn't alloc handle\n") ;
		goto exit;
	}
	
	if (!(tmr = openTimer())){
		printf("Failed to open timer resource\n");
		goto exit;
	}
	
	if (!(std.io_Message.mn_ReplyPort = CreateMsgPort())){
		goto exit;
	}else{
		std.io_Message.mn_Length = sizeof(struct IOStdReq) ;
		std.io_Message.mn_Node.ln_Type = NT_MESSAGE;
		std.io_Command = 0;
	}
	
	drv = (struct MinHandle*)handle;
	if (drv->version != buildVersion){
		printf("Driver version incorrect, this test built for 0x%04X, received 0x%04X\n", buildVersion, drv->version);
		goto exit;
	}
	
	sigmask = SIGBREAKF_CTRL_C | (1 << mySig) ;
	for ( ; ; ){
		sigs = timerWaitTO(tmr, 1,0,sigmask);
		if (sigs & (1 << mySig)){
			
		}
		if (sigs & SIGBREAKF_CTRL_C){
			break;
		}
		if (!sigs){
			// Timer completed
			doVUMeter(&std, drv->port);
			doInfo(&std, drv->port);
		}
	}
	
exit:
	if (std.io_Message.mn_ReplyPort){
		DeleteMsgPort(std.io_Message.mn_ReplyPort);
	}
	if (handle){
		MHIFreeDecoder  (handle);
	}
	if (mySig >=0){
		FreeSignal(mySig);
	}
	if (tmr){
		timerCloseTimer(tmr);
	}
	CloseLibrary(MHIBase);
}	