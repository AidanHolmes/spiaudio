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

int main (int argc, char **argv)
{
	struct Library *MHIBase = NULL ;
	APTR handle = NULL ;
	BYTE mySig = -1;
	
	if (!(MHIBase = OpenLibrary("mhispiaudio.library", 0))){
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
	
exit:
	if (handle){
		MHIFreeDecoder  (handle);
	}
	if (mySig >=0){
		FreeSignal(mySig);
	}
	CloseLibrary(MHIBase);
}	