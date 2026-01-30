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

#ifndef __H_LIBDEV_
#define __H_LIBDEV_
#include <exec/types.h>
#include <exec/resident.h>
#include <exec/exec.h>
#include <proto/exec.h>
#include <exec/semaphores.h>
#include "compatibility.h"

#ifndef LIBDEVNAME
#define LIBDEVNAME none.device
#endif

#ifndef LIBDEVMAJOR
#define LIBDEVMAJOR 1
#endif

#ifndef LIBDEVMINOR
#define LIBDEVMINOR 0
#endif

#ifndef LIBDEVDATE
#define LIBDEVDATE "1.1.2024"
#endif

#ifndef LIBDEV_VALIDATE_EXEC
#define LIBDEV_VALIDATE_EXEC 36
#endif

struct LibDevBase;

__SAVE_DS__ __ASM__ APTR     MHIAllocDecoder (__REG__(a0, struct Task*) task , __REG__(d0, ULONG) mhisignal, __REG__(a6, struct LibDevBase*) base);
__SAVE_DS__ __ASM__ VOID     MHIFreeDecoder  (__REG__(a3, APTR) handle, __REG__(a6, struct LibDevBase*) base);
__SAVE_DS__ __ASM__ BOOL     MHIQueueBuffer  (__REG__(a3, APTR) handle, __REG__(a0, APTR) buffer, __REG__(d0, ULONG) size, __REG__(a6, struct LibDevBase*) base);
__SAVE_DS__ __ASM__ APTR     MHIGetEmpty     (__REG__(a3, APTR) handle, __REG__(a6, struct LibDevBase*) base);
__SAVE_DS__ __ASM__ UBYTE    MHIGetStatus    (__REG__(a3, APTR) handle, __REG__(a6, struct LibDevBase*) base);
__SAVE_DS__ __ASM__ VOID     MHIPlay         (__REG__(a3, APTR) handle, __REG__(a6, struct LibDevBase*) base);
__SAVE_DS__ __ASM__ VOID     MHIStop         (__REG__(a3, APTR) handle, __REG__(a6, struct LibDevBase*) base);
__SAVE_DS__ __ASM__ VOID     MHIPause        (__REG__(a3, APTR) handle, __REG__(a6, struct LibDevBase*) base);
__SAVE_DS__ __ASM__ ULONG    MHIQuery        (__REG__(d1, ULONG) query, __REG__(a6, struct LibDevBase*) base);
__SAVE_DS__ __ASM__ VOID     MHISetParam     (__REG__(a3, APTR) handle, __REG__(d0, UWORD) param, __REG__(d1, ULONG) value, __REG__(a6, struct LibDevBase*) base);

/* Customise base according to requirements */
struct LibDevBase
{
	struct Device device;
	APTR seg_list;
	struct ExecBase *sys_base;
	/*
	struct Process *drvProc;
	struct Library *dosbase;
	struct MsgPort *drvPort;
	struct IORequest *timer ;
	struct SignalSemaphore accessIoSem;
	ULONG idle_sec;
	ULONG idle_microsec;
	BYTE sigTerm;
	struct IORequest *ioInProgress;
	BOOL abortInProgress;
	struct List clientInterrupts;
	UWORD *supportedCmds; // For NSD
	UWORD deviceType; // for NSD
	*/
	void *libData; // generic pointer to any remaining library/device data
};

#endif