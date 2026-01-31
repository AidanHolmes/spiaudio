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

#include "compatibility.h"
#include "vs1053.h"
#include "debug.h"
#include "patches/vs1053b-patches.plg"
#include "patches/vs1063a-patches.plg"
#if VS1053_AUDIO_ADMIX
#include "patches/admix-stereo.plg"
#endif

#define VERSION_VS1053		4
#define VERSION_VS1063		6

#define VSOP_WRITE 0x0200
#define VSOP_READ 0x0300

#define REG_MODE 		0x00
#define REG_STATUS 		0x01
#define REG_BASS 		0x02
#define REG_CLOCKF 		0x03
#define REG_DECODE_TIME 0x04
#define REG_AUDATA 		0x05
#define REG_WRAM 		0x06
#define REG_WRAMADDR 	0x07
#define REG_HDAT0 		0x08
#define REG_HDAT1 		0x09
#define REG_AIADDR 		0x0A
#define REG_VOL 		0x0B
#define REG_AICTRL0 	0x0C
#define REG_AICTRL1 	0x0D
#define REG_AICTRL2 	0x0E
#define REG_AICTRL3 	0x0F

#define SM_EARSPEAKERLO	0x0010 // not used in VS1063a
#define SM_EARSPEAKERHI	0x0080 // not used in VS1063a
#define SM_LAYER12		0x0002
#define SM_RESET		0x0004
#define SM_SDISHARE		0x0400
#define SM_SDINEW		0x0800
#define SM_CANCEL		0x0008
#define SM_LINE1		0x4000

#define SS_DO_NOT_JUMP	0x8000
#define SS_VUMETER		0x0200

// Memory locations
#define VSMEM_ENDFILLBYTE			0x1E06
#define ADDR_REG_GPIO_DDR_RW 		0xc017
#define ADDR_REG_GPIO_VAL_R  		0xc018
#define ADDR_REG_GPIO_ODATA_RW  	0xc019
#define ADDR_REG_I2S_CONFIG_RW  	0xc040
#define ADDR_REG_POSITIONMSEC		0x1E27
// VS1063 address locations
#define ADDR63_PLAYMODE 			0x1E09
#define ADDR63_ADMIX_GAIN			0x1E0D
#define ADDR63_ADMIX_CONFIG			0x1E0E
#define ADDR63_EARSPEAKER			0x1E1E
#define ADDR63_EQ5LEVEL1			0x1E13
#define ADDR63_EQ5FREQ1				0x1E14
#define ADDR63_EQ5LEVEL2			0x1E15
#define ADDR63_EQ5FREQ2				0x1E16
#define ADDR63_EQ5LEVEL3			0x1E17
#define ADDR63_EQ5FREQ3				0x1E18
#define ADDR63_EQ5LEVEL4			0x1E19
#define ADDR63_EQ5FREQ4				0x1E1A
#define ADDR63_EQ5LEVEL5			0x1E1B
#define ADDR63_EQ5UPDATE			0x1E1C
#define ADDR63_VUMETER				0x1E0C


#define VS63PLAYMODE_VUMETER			(1 << 2)
#define VS63PLAYMODE_ADMIX				(1 << 3)
#define VS63PLAYMODE_5BAND				(1 << 5)

#define VS63ADMIXER_RATEMASK 			(3 << 4)
#define VS63ADMIXER_RATE192 			(0 << 4)
#define VS63ADMIXER_RATE96 				(1 << 4)
#define VS63ADMIXER_RATE48 				(2 << 4)
#define VS63ADMIXER_RATE24 				(3 << 4)
#define VS63ADMIXER_MODEMASK 			(3 << 6)
#define VS63ADMIXER_MODESTEREO 			(0 << 6)
#define VS63ADMIXER_MODEMONO 			(1 << 6)
#define VS63ADMIXER_MODELEFT 			(2 << 6)
#define VS63ADMIXER_MODERIGHT			(3 << 6)

// Chunk size to send to VS1053. Can be 32 max, but due to SPIder lag
// this works best at 16 and then we capture full buffers in VS1053 earlier
#define CHUNK_SIZE		32

static void writeRegister(UBYTE address, UWORD data);
static UWORD readRegister(UBYTE address);
static BOOL waitDREQ_TO(struct VSData *dat, BOOL high, UWORD sec);
static UWORD writeMemoryWord(struct VSData *dat, UWORD address, UWORD value);
static UWORD readMemoryWord(struct VSData *dat, UWORD address);
static ULONG readMemoryLong(struct VSData *dat, UWORD address);

static void _resetList(struct List *l)
{
	l->lh_Tail = NULL;
	l->lh_Head = (struct Node *)&l->lh_Tail;
	l->lh_TailPred = (struct Node *)&l->lh_Head;
}

static void updateVolume(struct VSData *dat)
{
	// AmigaAmp works on a volume slider from -30db. Initial volume is 50 (out of 100)
	// VS10XX works down to -254db which isn't needed here. Anything of or below -33 is silence.
	// Separate into -3db per % of volume 
	ULONG lvol, rvol, vol ;
	
	//lvol = rvol = vol = (0x000000FE * (ULONG)dat->volume) / 100L;
	lvol = rvol = vol = dat->volume / 3 ;
	if (vol > 0){
		if (dat->panning > 50){
			lvol -= (vol*(dat->panning - 50))/50;
		}
		if (dat->panning < 50){
			rvol = (vol*(dat->panning))/50;
		}
	}
	// VS10X3 volumes are inverse from 0 max to FE silence.
	if (vol == 0){ // volume below -30db. Cut to zero after this.
		lvol = 254; 
		rvol = 254;
	}else{
		// invert volumes with 0 the loudest
		lvol = 33 - lvol ;
		rvol = 33 - rvol ;
	}
	D(DebugPrint(DEBUG_LEVEL, "updateVolume: request vol %u, setting left 0x%08X, setting right 0x%08X, reg 0x%04X\n", dat->volume, lvol, rvol, (UWORD)(lvol << 8 | rvol)));
	writeRegister(REG_VOL, (UWORD)(lvol << 8 | rvol));
}

static void initEqualiser(struct VSData *dat)
{
	UWORD i = 0, freqs[] = {64,250,1000,4000};
	if (dat->version == VERSION_VS1063){
		writeRegister(REG_BASS, 0) ; // switch off
		for (;i<4;i++){
			writeMemoryWord(dat, ADDR63_EQ5FREQ1+(i*2), freqs[i]);
		}
		writeMemoryWord(dat, ADDR63_EQ5UPDATE, 1);
	}
}

static void setEqualiser(struct VSData *dat, struct VSParameterData *param)
{
	WORD i=0, val ;
	
	if (param->value > 100){
		param->actual = 100;
	}
	val = ((param->actual / 3) - 16)*2;
	if (dat->version == VERSION_VS1053 && (param->parameter == VS_PARAM_BASS || param->parameter == VS_PARAM_TREBLE)){
		// Treble over 10kHz and bass under 60 Hz
		// 0 to 100 with 50 default/normal. Cannot cut so only 50-100 used and anything else is normal
		// HW supports 0-15 range for REG_BASS
		
		if (param->parameter == VS_PARAM_BASS){
			dat->bass = 0;
			if (param->actual > 50){
				dat->bass = (param->actual - 50) / 3;
				if (dat->bass > 15){
					dat->bass = 15;
				}
			}
		}else if (param->parameter == VS_PARAM_TREBLE){
			dat->treb = param->actual / 6 ;
			if (dat->treb > 15){
				dat->treb = 15;
			}
		}

		D(DebugPrint(DEBUG_LEVEL,"setEqualiser: VS1053 0x%04X\n", (dat->bass << 4) | (dat->treb << 12) | (dat->treb?(10 << 8):0) | (dat->bass?6:0)));
		writeRegister(REG_BASS, (dat->bass << 4) | (dat->treb << 12) | (dat->treb?(10 << 8):0) | (dat->bass?6:0));
	}else if (dat->version == VERSION_VS1063){
		// cut into 32 across 100 which is about 3 per percent. 64 is too fractional so just halve the range
		switch(param->parameter){
			case VS_PARAM_BASS: i=0; break;
			case VS_PARAM_MIDBASS: i=1; break;
			case VS_PARAM_MID: i=2; break;
			case VS_PARAM_MIDHIGH: i=3; break;
			case VS_PARAM_TREBLE: i=4; break;
			default:
				break;
		}
		
		if (val < -32){
			val = -32;
		}
		if (val > 32){
			val = 32;
		}
		D(DebugPrint(DEBUG_LEVEL,"setEqualiser: VS1063 eq %d = %d\n", i, val));
		writeMemoryWord(dat, ADDR63_EQ5LEVEL1+(i*2), val);
		writeMemoryWord(dat, ADDR63_EQ5UPDATE, 1);
	}
}

static void doAdmix(struct VSData *dat, struct VSParameterData *param)
{
	BYTE gain;
	
	gain = (param->value & 0x00FF);
	
	if (gain > -3){
		// This is a status query
		if (dat->version == VERSION_VS1053){
			param->actual = (BYTE)readRegister(REG_AICTRL0);
		}else if (dat->version == VERSION_VS1063){
			param->actual = (BYTE)readMemoryWord(dat, ADDR63_ADMIX_GAIN);
		}
		param->actual |= dat->admix?0x0100:0x0000;
	}else{
		if ((param->value & 0xFF00) != 0){
			// Disable
			if (dat->version == VERSION_VS1053){
				writeRegister(REG_AIADDR, 0x0F01);
			}else if (dat->version == VERSION_VS1063){
				if (dat->admix){
					dat->playMode &= ~VS63PLAYMODE_ADMIX ;
					writeMemoryWord(dat, ADDR63_PLAYMODE, dat->playMode);
					dat->admix = FALSE ;
				}
			}
		}else{
			// Enable
			if (dat->version == VERSION_VS1053){
				writeRegister(REG_AIADDR, 0x0F00);
				writeRegister(REG_AICTRL0, (WORD)gain);
			}else if (dat->version == VERSION_VS1063){
				if (!dat->admix){
					dat->playMode |= VS63PLAYMODE_ADMIX ;
					writeMemoryWord(dat, ADDR63_PLAYMODE, dat->playMode);
					dat->admix = TRUE ;
				}
				writeMemoryWord(dat, ADDR63_ADMIX_GAIN, (WORD)gain);
			}
		}
		param->actual = param->value;
	}
}

static void doParameter(struct VSData *dat, struct IOStdReq *std)
{
	struct VSParameterData *param = NULL;
	BYTE earspeaker = 0;
	
	param = (struct VSParameterData*)std->io_Data;
	param->actual = 0; // zero for any errors
	
	if (std->io_Length != sizeof(struct VSParameterData)){
		std->io_Error = IOERR_BADLENGTH; // use standard errors from exec/error.h
		return;
	}
	
	param->actual = param->value; // set to incoming value - updated later if required
	
	switch(param->parameter){
		case VS_PARAM_VOL:
			dat->volume = param->value ;
			if (param->value > 100){
				dat->volume = 100;
			}
			updateVolume(dat);
			param->actual = dat->volume;
			break;
		case VS_PARAM_PAN:
			dat->panning = param->value ;
			if (param->value > 100){
				dat->panning = 100;
			}
			updateVolume(dat);
			param->actual = dat->panning;
			break;
		case VS_PARAM_CROSSMIX:
			dat->crossmixing = param->value ;
			if (param->value > 100){
				dat->crossmixing = 100;
			}
			// Don't actually do crossmixing, but can modify headphone setting
			// Split into the 4 supported modes
			earspeaker = dat->crossmixing / 25;
			if (earspeaker >= 4){
				earspeaker = 3;
			}
			if (dat->version == 4){
				writeRegister(REG_MODE, readRegister(REG_MODE) | ((earspeaker & 0x01)?SM_EARSPEAKERLO:0) | ((earspeaker & 0x02)?SM_EARSPEAKERHI:0));
			}
			if (dat->version == 6){
				writeMemoryWord(dat, ADDR63_EARSPEAKER, 500 * dat->crossmixing);
			}
			break;
		case VS_PARAM_BASS:
		case VS_PARAM_TREBLE:
		case VS_PARAM_MID:
		case VS_PARAM_MIDBASS:
		case VS_PARAM_MIDHIGH:
			setEqualiser(dat, param);
			break;
		case VS_PARAM_ADMIX:
			doAdmix(dat, param);
			break;
		case VS_PARAM_VUMETER:
			if(dat->version == VERSION_VS1053){
				param->actual = readRegister(REG_AICTRL3); // 1db resolution
			}else if (dat->version == VERSION_VS1063){
				param->actual = readMemoryWord(dat, ADDR63_VUMETER); // 3db resolution
				param->actual *= 3; // align to same range as VS1053 0-96
			}
			break;
		default:
			break;
	}
}

static void getMediaInfo(struct VSData *dat, struct IOStdReq *std)
{
	struct VSMediaInfo *info = NULL;
	
	info = (struct VSMediaInfo*)std->io_Data;
	
	if (std->io_Length != sizeof(struct VSMediaInfo)){
		std->io_Error = IOERR_BADLENGTH; // use standard errors from exec/error.h
		return;
	}
	info->hwVersion = dat->version;
	info->hdat0 = readRegister(REG_HDAT0);
	info->hdat1 = readRegister(REG_HDAT1);
	info->audata = readRegister(REG_AUDATA);
	info->positionMsec = (LONG)readMemoryLong(dat, ADDR_REG_POSITIONMSEC);
}

static void __saveds workerTask(void)
{
	struct Task *thisTask = NULL;
	struct VSData *dat = NULL ;
	//struct ExecBase *SysBase = *(struct ExecBase **)4UL;
	BOOL bSetupOK = FALSE;
	ULONG sigs = 0, sigmask = 0;
	struct IOStdReq *std = NULL ;
	struct IORequest *ior = NULL;
	
	thisTask = FindTask(NULL);
	
	dat = (struct VSData*)thisTask->tc_UserData;
	
	dat->sigTerm = AllocSignal(-1);
	if (dat->sigTerm < 0){
		D(DebugPrint(ERROR_LEVEL,"workerTask: failed to allocate terminate signal. Exiting...\n"));
		goto close; // reply to startup code with no devPort to kill everything
	}
	
	if ((dat->sig = AllocSignal(-1)) < 0){
		D(DebugPrint(ERROR_LEVEL, "workerTask: couldn't allocate signal\n"));
		goto close;
	}
	
	if (!(dat->tmr = openTimer())){
		D(DebugPrint(ERROR_LEVEL, "workerTask: couldn't open timer\n"));
		goto close;
	}
	
	if (spi_initialize(&dat->config, dat->sig) != 0){
		D(DebugPrint(ERROR_LEVEL, "workerTask: SPI initialisation failed\n"));
		goto close;
	}
	dat->initspi = TRUE ;
#if VS10XX_USE_INTERRUPTS
	spi_enable_interrupt();
#else
	spi_disable_interrupt();
#endif
	
	// Setup message port to handle new messages
	if (!(dat->drvPort = CreateMsgPort())){
		D(DebugPrint(ERROR_LEVEL,"workerTask: creation of message port failed\n"));
		goto close;
	}
	
	// issue hard reset
	spider_usr_reset(0);
	spider_usr_reset(1);
	if (!waitDREQ_TO(dat, TRUE, 5)){ // wait for hw to come back up after reset
		D(DebugPrint(ERROR_LEVEL, "workerTask: failed to hard reset\n"));
		goto close;
	}
	
	// Reset the device here as the worker owns the signal interrupt (waits forever in the calling process)
	if (!resetVS1053(dat)){
		goto close;
	}
	bSetupOK = TRUE;

	D(DebugPrint(DEBUG_LEVEL,"workerTask: completed setup, signal parent ready\n"));
	Signal(dat->callingTask , (1 << dat->callingSig));
	sigmask = (1 << dat->drvPort->mp_SigBit) | (1 << dat->sig) | (1 << dat->sigTerm);
	while (bSetupOK){
		//D(DebugPrint(DEBUG_LEVEL, "workerTask: dat->status 0x%04X\n", dat->status));
		if (((dat->status & (VS_PLAYING | VS_PAUSED)) == VS_PLAYING) && !(dat->status & VS_NOBUFF) || (dat->status & VS_STOPPING) > 0){
			//D(DebugPrint(DEBUG_LEVEL, "workerTask: dat->status sending sig for 0x%04X\n", dat->status));
			sigs = SetSignal(0L,0L) & sigmask; // Peek at waiting signals we are interested in
			if (sigs){
				// Wait and clear signals properly
				sigs = Wait(sigmask);
			}
			sigs |= 1 << dat->sig; // Pretend we had a signal for VS1053
		}else{
			//D(DebugPrint(DEBUG_LEVEL, "workerTask: waiting - dat->status 0x%04X\n", dat->status));
			// Not playing or stopping, so chill on normal received signals
			sigs = timerWaitTO(dat->tmr, 1,0,sigmask) ; // 1 sec idle
		}
		if (sigs & (1 << dat->drvPort->mp_SigBit)){
			while (ior = (struct IORequest *)GetMsg(dat->drvPort)){
				ior->io_Flags &= ~IOF_QUICK;
				ior->io_Error = 0;
				std = (struct IOStdReq*)ior;
				
				switch (ior->io_Command){
					case CMD_START: // Play data in buffer
						D(DebugPrint(DEBUG_LEVEL,"workerTask: CMD_START\n"));
						if (dat->status & VS_PLAYING){
							// Already playing, unpause 
							dat->status &= ~VS_PAUSED;
						}else{
							dat->status = VS_PLAYING | VS_NEWSTART;
						}
						playChunk(dat);
						break;
					case CMD_STOP: // Pause playing of data in buffer
						D(DebugPrint(DEBUG_LEVEL,"workerTask: CMD_STOP\n"));
						pausePlayback(dat, TRUE);
						break;
					case CMD_RESET: // Stop and reset stream for new file to play
						D(DebugPrint(DEBUG_LEVEL,"workerTask: CMD_RESET\n"));
						dat->status = VS_STOPPING; // clear anyother status and flag to just stop
						break; 
					case CMD_VSAUDIO_PARAMETER:
						doParameter(dat, std);
						break;
					case CMD_VSAUDIO_INFO:
						getMediaInfo(dat, std);
						break;
					default:
						ior->io_Error = IOERR_NOCMD;
						break;
				}
				
				ReplyMsg(&ior->io_Message);
			}
		}else if(sigs & (1 << dat->sig)){
			// Device signal for data
			if (dat->status & VS_PLAYING){
				playChunk(dat);
			}
			if(dat->status & VS_STOPPING){
				if (stopPlayback(dat)){
					ObtainSemaphore(&dat->sem);
					if (dat->currentChunk){
						AddHead(&dat->bufferList, (struct Node*)dat->currentChunk); // put back on list
						dat->currentChunk = NULL;
					}
					ReleaseSemaphore(&dat->sem);
				}
			}
		}else{
			// Watchdog timer triggered

		}
		
		if(sigs & (1 << dat->sigTerm)){
			bSetupOK = FALSE ; // exit
		}
	}
	
close:
	///////////////////////////////////////
	// Terminate working process
	D(DebugPrint(DEBUG_LEVEL,"workerTask: terminating...\n"));
	
	if (dat->initspi){
		spi_shutdown();
		dat->initspi = FALSE;
	}
	if (dat->drvPort){
		DeleteMsgPort(dat->drvPort);
		dat->drvPort = NULL ;
	}
	if (dat->sigTerm >=0){
		FreeSignal(dat->sigTerm);
		dat->sigTerm = -1;
	}
	if (dat->sig >=0){
		FreeSignal(dat->sig);
		dat->sig = -1;
	}
	if (dat->tmr){
		timerCloseTimer(dat->tmr);
		dat->tmr = NULL;
	}
	
	removeAllChunks(dat);
	FreeVec(dat); // memory can only be cleanly removed here
}

__INLINE__ static BOOL dreq(void)
{
	return (BOOL)((spi_pin_val(PIN_INT) == 1)?TRUE:FALSE);
}

__INLINE__ static BOOL waitDREQ(struct VSData *dat, BOOL high)
{
	ULONG sigs = 0;
	
#if !VS10XX_USE_INTERRUPTS
	setTimer(dat->tmr, 1, 0);
#endif 
	while(dreq() != high){
#if VS10XX_USE_INTERRUPTS
		sigs = timerWaitTO(dat->tmr, 1, 0, 1 << dat->sig); // wait completed or timer ended
		if (!sigs && (dreq() != high)){
#else
		if(CheckIO(dat->tmr)){ // Timer completed in polling mode
			WaitIO(dat->tmr) ; // clear timer (should return immediately)
#endif
			D(DebugPrint(DEBUG_LEVEL,"waitDREQ: timed out with dreq %d and waiting for it to be %d\n", dreq(), high));
			return FALSE ; // timed out
		}
	}
#if !VS10XX_USE_INTERRUPTS
	AbortIO(dat->tmr); // clear the timer started in polling loop
#endif
	return TRUE ;
}

__INLINE__ static BOOL waitDREQ_TO(struct VSData *dat, BOOL high, UWORD sec)
{
	ULONG sigs = 0;
	
#if !VS10XX_USE_INTERRUPTS
	setTimer(dat->tmr, 1, 0);
#endif 
	while(dreq() != high){
#if VS10XX_USE_INTERRUPTS
		sigs = timerWaitTO(dat->tmr, sec, 0, 1 << dat->sig); // wait completed or timer ended
		if (!sigs && (dreq() != high)){
#else
		if(CheckIO(dat->tmr)){ // Timer completed in polling mode
			WaitIO(dat->tmr) ; // clear timer (should return immediately)
#endif
			D(DebugPrint(DEBUG_LEVEL,"waitDREQ: timed out with dreq %d and waiting for it to be %d\n", dreq(), high));
			return FALSE ; // timed out
		}
	}
#if !VS10XX_USE_INTERRUPTS
	AbortIO(dat->tmr); // clear the timer started in polling loop
#endif
	return TRUE ;
}

__INLINE__ static void writeRegister(UBYTE address, UWORD data)
{
	UWORD outw[2] = {VSOP_WRITE,0x00};
	
	outw[0] |= address ;
	outw[1] = data;

	spi_select();
	spi_write((unsigned char *)outw, 4);
	spi_deselect();
}

__INLINE__ static void writeMultiRegister(struct VSData *dat, UBYTE address, UWORD *data, UWORD len)
{
	UWORD outw[2] = {VSOP_WRITE,0x00};
	
	outw[0] |= address ;
	outw[1] = *data++;
	
	spi_select();
	
	spi_write((unsigned char*)outw, 4);
	for (--len;len != 0;--len){
		// wait for DREQ to go high before next word
		waitDREQ(dat, TRUE); // TO DO check return for timeouts?
		spi_write((unsigned char*)data++, 2);
	}
	
	spi_deselect();
}

static void loadCompressedPatch(const UWORD *plugin, const ULONG length)
{
  int i = 0;
  unsigned short addr, n, val;

  while (i<(length/2)) {
    addr = plugin[i++];
    n = plugin[i++];
    if (n & 0x8000U) { /* RLE run, replicate n samples */
      n &= 0x7FFF;
      val = plugin[i++];
      while (n--) {
        writeRegister((UBYTE)addr, val);
      }
    } else {           /* Copy run, copy n samples */
      while (n--) {
        val = plugin[i++];
        writeRegister((UBYTE)addr, val);
      }
    }
  }
}

__INLINE__ static UWORD readRegister(UBYTE address)
{
	UWORD outw = VSOP_READ;
	UWORD inw ;
	outw |= address;
	
	spi_select();
	spi_write((unsigned char*)&outw, 2);
	spi_read((unsigned char*)&inw, 2);
	spi_deselect();
	
	return inw;
}

__INLINE__ static UWORD readMemoryWord(struct VSData *dat, UWORD address)
{
	writeRegister(REG_WRAMADDR, address);
	if (!waitDREQ(dat, TRUE)){
		D(DebugPrint(ERROR_LEVEL, "readMemoryWord: failed WRAM read\n"));
		return FALSE ;
	}

	return readRegister(REG_WRAM);
}

__INLINE__ static UWORD writeMemoryWord(struct VSData *dat, UWORD address, UWORD value)
{
	writeRegister(REG_WRAMADDR, address);
	if (!waitDREQ(dat, TRUE)){
		D(DebugPrint(ERROR_LEVEL, "writeMemoryWord: failed WRAM address change\n"));
		return FALSE ;
	}

	writeRegister(REG_WRAM, value);
	return TRUE ;
}

__INLINE__ static ULONG readMemoryLong(struct VSData *dat, UWORD address)
{
	UWORD hi[2], lo[2];
	UBYTE i=0;
	hi[i] = readMemoryWord(dat,address);
	lo[i] = readRegister(REG_WRAM);
	
	// Attempt stable match or give up and return values read (may be wrong!)
	for(i=1; i < 4; i++){
		hi[i&0x01] = readMemoryWord(dat,address);
		lo[i&0x01] = readRegister(REG_WRAM);
		if(hi[0] == hi[1] && lo[0] == lo[1]){
			break;
		}
	}
	
	return ((ULONG)hi[0] << 4) | (ULONG)lo[0];
}

static BOOL switchToMp3Mode(struct VSData *dat)
{
    writeMemoryWord(dat, ADDR_REG_GPIO_DDR_RW, 3); // GPIO DDR = 3
    writeMemoryWord(dat, ADDR_REG_GPIO_ODATA_RW, 0); // GPIO ODATA = 0

    return resetVS1053(dat);
}

static void sendEndBytes(struct VSData *dat, ULONG len) 
{
    int readbytes = 0, i=0; // Length of chunk CHUNK_SIZE byte or shorter

    for (readbytes =0; len != 0; len -= readbytes){ // More to do?
        if (!waitDREQ(dat, TRUE)){
			D(DebugPrint(ERROR_LEVEL, "sendEndBytes: DREQ failed\n"));
			return;
		}
		readbytes = len > CHUNK_SIZE?CHUNK_SIZE:len;

        for (i=0; i < readbytes; i++){
			spi_write(&dat->endfill,1);
		}
    }
  
}

static struct Task* createWorkerTask(struct VSData *dat, char *taskName, LONG priority, APTR funcEntry, ULONG stackSize, APTR userData)
{
	struct	MemList *mem = NULL;
	BOOL success = FALSE;
	struct Task *newTask = NULL ;
	struct MemListTask{
		struct MemList _mem;
		struct MemEntry ml_ME2; // second entry
	} memList;
	
	if (!(newTask = (struct Task*)AllocVec(sizeof(struct Task), MEMF_ANY | MEMF_CLEAR))){
		D(DebugPrint(ERROR_LEVEL,"createWorkerTask: failed to allocate task\n"));
		goto close;
	}
	memList._mem.ml_NumEntries = 2;
	memList._mem.ml_ME[0].me_Un.meu_Reqs = MEMF_PUBLIC|MEMF_CLEAR;
	memList._mem.ml_ME[0].me_Length = sizeof(struct Task);
    memList._mem.ml_ME[1].me_Un.meu_Reqs = MEMF_ANY|MEMF_CLEAR;
    memList._mem.ml_ME[1].me_Length = stackSize;
	
	mem = AllocEntry((struct MemList *)&memList);
	if ((ULONG)mem & (1<<31)) {
		D(DebugPrint(ERROR_LEVEL,"createWorkerTask: failed to allocate task memory\n"));
		goto close;
	}
	
	newTask = mem->ml_ME[0].me_Un.meu_Addr;
	newTask->tc_SPLower      = mem->ml_ME[1].me_Un.meu_Addr;
	newTask->tc_SPUpper      = (UBYTE *)(mem->ml_ME[1].me_Un.meu_Addr) + stackSize;
	newTask->tc_SPReg        = newTask->tc_SPUpper;
	newTask->tc_UserData     = userData;
	newTask->tc_Node.ln_Name = taskName;
	newTask->tc_Node.ln_Type = NT_TASK;
	newTask->tc_Node.ln_Pri  = priority;
	_resetList(&newTask->tc_MemEntry);
	AddHead(&newTask->tc_MemEntry,(struct Node *)mem);

	AddTask(newTask,funcEntry,NULL);
	
	success = TRUE ;
close:
	if (!success){
		if (mem){
			if ((ULONG)mem & (1<<31) == 0){
				FreeEntry(mem);
			}
		}
		if (newTask){
			FreeVec(newTask);
			newTask = NULL;
		}
	}
	return newTask;
}

BOOL startPlaying(struct VSData *dat)
{
	//writeRegister(REG_AUDATA, 44101) ;
	sendEndBytes(dat, 10);
	
	//if (!switchToMp3Mode(dat)){
	//	D(DebugPrint(ERROR_LEVEL, "playBuffer: couldn't switch to MP3 mode\n"));
	//	return FALSE ;
	//}
	return TRUE ;
}

/////////////////////////////////////////////////////////////////////////
//
// Public functions
//
//
//

/****** vs1053/updateChunk *****************************************************
*
* NAME
*     resetChunk -- Add new attributes for an existing chunk
*
* SYNOPSIS
*     updateChunk(struct VSData *dat, struct VSChunk *chunk, ULONG size, 
                  struct Task *task, ULONG sigs);
* 
* FUNCTION
*     Recycle existing chunk buffer with new data, task and signal.
*
* INPUTS
*     struct VSData *dat - instance from initVS1053
*     struct VSChunk *chunk - allocated chunk
*     ULONG size - new size of chunk
*     struct Task* - pointer to calling task 
*     ULONG sigs - signal mask for calling task to receive updates
*
* RESULT
*     VOID - no return
* 
* NOTES
*     Sets to zero. Useful for any chunk returned by getUsedChunk to reset all
*     attributes before updateChunk called. 
* 
* SEE ALSO
*     updateChunk(...), getUsedChunk
*
*******************************************************************************
*
*/
__INLINE__ void updateChunk(struct VSData *dat, struct VSChunk *chunk, ULONG size, struct Task *task, ULONG sigs)
{
	chunk->offset = 0;
	chunk->size = size;
	chunk->sigs = sigs;
	chunk->ownerTask = task;
	ObtainSemaphore(&dat->sem);
	AddTail(&dat->bufferList, (struct Node*)chunk);
	ReleaseSemaphore(&dat->sem);
}

/****** vs1053/resetChunk *****************************************************
*
* NAME
*     resetChunk -- Reset chunk to zero, no task owner and signal. 
*
* SYNOPSIS
*     resetChunk(struct VSChunk *chunk)
* 
* FUNCTION
*     Clear the attributes of a chunk, but not the associated buffer
*
* INPUTS
*     struct VSChunk *chunk - allocated chunk
*
* RESULT
*     VOID - no return
* 
* NOTES
*     Sets to zero. Useful for any chunk returned by getUsedChunk to reset all
*     attributes before updateChunk called. 
* 
* SEE ALSO
*     updateChunk(...), getUsedChunk
*
*******************************************************************************
*
*/
__INLINE__ void resetChunk(struct VSChunk *chunk)
{
	// Retain node and buffer information
	chunk->offset = 0;
	chunk->size = 0;
	chunk->sigs = 0;
	chunk->ownerTask = NULL;
	
}

/****** vs1053/removeAllChunks *****************************************************
*
* NAME
*     removeAllChunks -- Delete all chunk buffers
*
* SYNOPSIS
*     removeAllChunks(struct VSChunk *chunk)
* 
* FUNCTION
*     Delete a chunk returned by getUsedChunk. 
*
* INPUTS
*     struct VSData *dat - instance from initVS1053
*
* RESULT
*     VOID - no return
* 
* NOTES
*     Deletes the memory for all chunks. Note that this will not delete memory
*     allocated for the buffers and caller must manage this separately
* 
* SEE ALSO
*     removeChunk(...)
*
*******************************************************************************
*
*/
__INLINE__ void removeAllChunks(struct VSData *dat)
{
	struct Node *n = NULL;
	while ( n = RemHead((struct List *)&dat->bufferList) ){
		FreeVec(n);
	}
	while ( n = RemHead((struct List *)&dat->freeList) ){
		FreeVec(n);
	}
	if (dat->currentChunk){
		FreeVec(dat->currentChunk);
		dat->currentChunk = NULL ;
	}
}

/****** vs1053/removeChunk *****************************************************
*
* NAME
*     removeChunk -- Delete chunk
*
* SYNOPSIS
*     removeChunk(struct VSChunk *chunk)
* 
* FUNCTION
*     Delete a chunk returned by getUsedChunk. 
*
* INPUTS
*     struct VSChunk *chunk - chunk data to delete
*
* RESULT
*     VOID - no return
* 
* NOTES
*     Deletes the memory for the chunk. Note that this will not delete memory
*     allocated for the buffer and caller must manage this separately
* 
* SEE ALSO
*     updateChunk(...), getUsedChunk(...)
*
*******************************************************************************
*
*/
__INLINE__ void removeChunk(struct VSChunk *chunk)
{
	Remove((struct Node*)chunk);
	FreeVec(chunk); 
}

/****** vs1053/getUsedChunk *****************************************************
*
* NAME
*     getUsedChunk -- Get a completed chunk
*
* SYNOPSIS
*     getUsedChunk(struct VSData *dat)
* 
* FUNCTION
*     Return a completed chunk for the caller to decide to reuse or discard
*
* INPUTS
*     struct VSData *dat - instance from initVS1053
*
* RESULT
*     struct VSChunk* - valid pointer to completed chunk or NULL if none available
* 
* NOTES
*     Completed chunks are queued and calling process/task should call this 
*     function to remove and recycle (or delete). Calling this function will 
*     remove the chunk from the available queue. Call updateChunk to recycle
*     with new buffer content or removeChunk to delete
* 
* SEE ALSO
*     updateChunk(...), removeChunk(struct VSData*)
*
*******************************************************************************
*
*/
struct VSChunk* getUsedChunk(struct VSData *dat)
{
	struct VSChunk *returnthis = NULL;
	
	ObtainSemaphore(&dat->sem);
	returnthis = (struct VSChunk*)RemHead(&dat->freeList);
	ReleaseSemaphore(&dat->sem);
	
	if (returnthis){
		return returnthis;
	}
	return NULL;	
}

/****** vs1053/stopPlayback *****************************************************
*
* NAME
*     stopPlayback -- Stop playback of a song
*
* SYNOPSIS
*     stopPlayback(struct VSData *dat)
* 
* FUNCTION
*     Stop playback by sending cancel song sequence to VS1053.
*
* INPUTS
*     struct VSData *dat - instance from initVS1053
*
* RESULT
*     BOOL - TRUE, the function worked as expected and stopped
*            FALSE, not really returned, but would suggest a failure (hardware)
* 
* NOTES
*     Handles appropriate padding and flags required to stop neatly.
*     Recommended to raise CMD_RESET to the worker thread to issue a stop of a
*     playback.
* 
* SEE ALSO
*     CMD_START, CMD_RESET
*
*******************************************************************************
*
*/
BOOL stopPlayback(struct VSData *dat)
{
	if (readRegister(REG_STATUS) & SS_DO_NOT_JUMP){
		return FALSE;
	}
	ObtainSemaphore(&dat->sem);
	
	sendEndBytes(dat,2052);
	writeRegister(REG_MODE, readRegister(REG_MODE) | SM_CANCEL) ;
	sendEndBytes(dat,32);
	if (readRegister(REG_MODE) & SM_CANCEL){
		sendEndBytes(dat,32);
	}
	if (readRegister(REG_MODE) & SM_CANCEL){
		resetVS1053(dat);
	}
	
	dat->status = 0; // stopped with no flags set
	
	ReleaseSemaphore(&dat->sem);
	
	return TRUE ;
}

/****** vs1053/pausePlayback *****************************************************
*
* NAME
*     pausePlayback -- Pause the audio playback
*
* SYNOPSIS
*     pausePlayback(struct VSData *dat, BOOL pause)
* 
* FUNCTION
*     Pause playback. Sets appropriate flag to pause  playback.
*
* INPUTS
*     struct VSData *dat - instance from initVS1053
*     BOOL pause - set to TRUE to pause and FALSE to unpause
*
* RESULT
*     VOID - no return
* 
* NOTES
*     Call this function to signal pause or unpause actions
*     
*
*******************************************************************************
*
*/
__INLINE__ void pausePlayback(struct VSData *dat, BOOL pause)
{
	if (pause){
		dat->status |= VS_PAUSED;
	}else{
		dat->status &= ~VS_PAUSED;
	}
}

/****** vs1053/addBuffer *****************************************************
*
* NAME
*     addBuffer -- Add a new buffer to the playback queue
*
* SYNOPSIS
*     BOOL = addBuffer(struct VSData *dat, APTR buffer, ULONG size, struct Task *task, ULONG sigs)
* 
* FUNCTION
*     Add new buffer to playback queue. 
*
* INPUTS
*     struct VSData *dat - instance from initVS1053
*     APTR buffer - buffer which caller has allocated and written data to
*     ULONG size - size of the allocated and provided buffer
*     struct Task *task - caller task pointer, used for signalling of completed buffer
*     ULONG sigs - signal mask for calling task to receive completed buffer signal
*
* RESULT
*     BOOL - TRUE, the function worked as expected and buffer queued
*            FALSE, something went wrong (e.g. no memory to allocate new buffer)
* 
* NOTES
*     Call this function to load up buffer data for playback. Fist in and first out data loading
* 
* SEE ALSO
*     removeChunk(struct VSChunk*), removeAllChunks(struct VSData),
*     getUsedChunk(struct VSData*)
*
*******************************************************************************
*
*/
__INLINE__ BOOL addBuffer(struct VSData *dat, APTR buffer, ULONG size, struct Task *task, ULONG sigs)
{
	struct VSChunk *buf;
	buf = (struct VSChunk*)AllocVec(sizeof(struct VSChunk), MEMF_ANY | MEMF_CLEAR);
	if (!buf){
		return FALSE ;
	}
	ObtainSemaphore(&dat->sem);
	dat->lastChunkTask = task;
	dat->lastChunkSigs = sigs;
	updateChunk(dat, buf, size, task, sigs);
	buf->buffer = buffer; // Only attribute not managed by updateChunk or resetChunk
	ReleaseSemaphore(&dat->sem);
	
	return TRUE ;
}

/****** vs1053/playChunk *****************************************************
*
* NAME
*     playChunk -- Play next loaded chunk 
*
* SYNOPSIS
*     BOOL = playChunk(struct VSData);
* 
* FUNCTION
*     Play a pending chunk of data by sending to VS1053. Must be called 
*     repeatably to push data. This function will wait until VS1053 is ready.
*     This function will do nothing if VSData status isn't VS_PLAYING.
*
* INPUTS
*     struct VSData* - instance from initVS1053
*
* RESULT
*     BOOL - TRUE, the function worked as expected (even if not VS_PLAYING)
*            FALSE, something went wrong and couldn't play (timeout hw error)
* 
* NOTES
*     Manages either playback or status updates in VSData if it cannot play.
*     New songs require VS_NEWSTART status flag setting to create preamble data
* 
* SEE ALSO
*     destroyVS1053()
*
*******************************************************************************
*
*/
BOOL playChunk(struct VSData *dat)
{
	ULONG remaining = 0, readbytes = 0;
	
	if (!(dat->status & VS_PLAYING)){
		return TRUE ; // no flag set to play - normal response and not an error
	}
	
	if (dat->status & VS_NEWSTART){
		if (!startPlaying(dat)){
			return FALSE;
		}
		dat->status &= ~VS_NEWSTART; // clear flag of new start
	}
	
	if (dat->status & VS_PAUSED){
		// Do nothing, volume will drop with no buffer
	}else{
		
		if (!dat->currentChunk){
			if (IsListEmpty(&dat->bufferList)){
				if (!(dat->status & VS_NOBUFF)){
					dat->stdPriority = SetTaskPri(dat->drvTask, -10); // Drop the priority as it can hog the scheduler
					dat->status |= VS_NOBUFF;
				}
				D(DebugPrint(DEBUG_LEVEL,"*")); // Symbol for very empty buffer
				if (dat->lastChunkTask){ // Send another signal is lastChunkTask set (we had previous buffers)
					D(DebugPrint(DEBUG_LEVEL,"@")); // Symbol for signal
					D(DebugPrint(DEBUG_LEVEL,"Signal: %p, 0x%08X\n", dat->lastChunkTask, dat->lastChunkSigs)); 
					Signal(dat->lastChunkTask, dat->lastChunkSigs);
				}
			}else{
				ObtainSemaphore(&dat->sem);
				if ((dat->currentChunk=(struct VSChunk*)RemHead(&dat->bufferList))){
					if (dat->status & VS_NOBUFF){ // if we had a nobuff situation then restore priority and status
						D(DebugPrint(DEBUG_LEVEL,"SetTaskPri: %p, %d\n", dat->drvTask, dat->stdPriority)); 
						SetTaskPri(dat->drvTask, dat->stdPriority);
						dat->status &= ~VS_NOBUFF; // clear no buff flag
					}
				}
				ReleaseSemaphore(&dat->sem);
			}
		}
		
		if (dat->currentChunk){
			remaining = (dat->currentChunk->size - dat->currentChunk->offset);
			if ((readbytes = remaining > CHUNK_SIZE?CHUNK_SIZE:remaining)){
				if (!waitDREQ(dat, TRUE)){
					D(DebugPrint(ERROR_LEVEL, "playChunk: DREQ failed\n"));
					
					return FALSE ;
				}
				D(DebugPrint(DEBUG_LEVEL,"-")); // Symbol for used buffer
				//D(DebugPrint(DEBUG_LEVEL,"playChunk: sending %u bytes from chunk %p at offset %u\n", readbytes, dat->currentChunk, dat->currentChunk->offset));
				spi_write((unsigned char*)(dat->currentChunk->buffer + dat->currentChunk->offset),readbytes);
			}
			dat->currentChunk->offset += readbytes ;
			
			// Check buffer has completed and recirculate back into the List of buffers
			if (dat->currentChunk->offset >= dat->currentChunk->size){
				ObtainSemaphore(&dat->sem);
				AddTail(&dat->freeList, (struct Node *)dat->currentChunk); // add to free chunk list	
				ReleaseSemaphore(&dat->sem);
				// Signal to owner that a buffer has completed
				//D(DebugPrint(DEBUG_LEVEL,"Sig: task %p, sig %d", dat->currentChunk->ownerTask, dat->currentChunk->sigs));
				if (dat->currentChunk->ownerTask && dat->currentChunk->sigs > 0){
					D(DebugPrint(DEBUG_LEVEL,"@")); // Symbol for signal
					Signal(dat->currentChunk->ownerTask, dat->currentChunk->sigs);
				}
				//resetChunk(dat->currentChunk); // Reset attributes but keep buffer for client (we don't own this)
				dat->currentChunk = NULL ; // Clear current working chunk for allocation in next try
			}
		}
	}
	return TRUE ;
}

/****** vs1053/resetVS1053 *****************************************************
*
* NAME
*     resetVS1053 -- Issue soft reset to VS1053 
*
* SYNOPSIS
*     BOOL = resetVS1053(struct VSData*);
* 
* FUNCTION
*     Soft reset the VS1053 and apply patches.
*
* INPUTS
*     struct VSData* - allocated from initVS1053
*
* RESULT
*     BOOL - TRUE when success, FALSE if fails
* 
* NOTES
*     Can fail if hardware issues or bus issues experienced. Suggest full reset
*     of hardware on failure
* 
*
*******************************************************************************
*
*/
BOOL resetVS1053(struct VSData *dat)
{
	UWORD newMode = 0 ;
	// TO DO: this func could send message to worker task to reset CMD_RESET
	// as it cannot work in other processes/tasks as they do not have the right task and signals
	
	spi_set_speed(SPI_SPEED_SLOW); // SPI_SPEED_FAST
	
	timer_delay(TIMER_MILLIS(1)); // let clock and stuff settle
	
	writeRegister(REG_MODE, SM_RESET | SM_SDINEW | SM_SDISHARE);
	if (!waitDREQ(dat, TRUE)){
		D(DebugPrint(ERROR_LEVEL, "resetVS1053: failed soft reset\n"));
		return FALSE ;
	}
	
	// Set modes
	writeRegister(REG_MODE, readRegister(REG_MODE) | SM_SDISHARE | SM_SDINEW);
	
	dat->version = (readRegister(REG_STATUS) & 0x00F0) >> 4;
	if (dat->version != VERSION_VS1053 && dat->version != VERSION_VS1063){
		D(DebugPrint(ERROR_LEVEL, "initVS1053: invalid hardware. Expecting version 4 or 6, read version %u\n", dat->version));
		return FALSE;
	}
	
	if (dat->version == VERSION_VS1053){
		// Stop start up of VS1053 in MIDI mode
		writeMemoryWord(dat, ADDR_REG_GPIO_DDR_RW, 3); // GPIO DDR = 3
		writeMemoryWord(dat, ADDR_REG_GPIO_ODATA_RW, 0); // GPIO ODATA = 0
	}
	
	dat->endfill = (UBYTE)(readMemoryWord(dat,VSMEM_ENDFILLBYTE) & 0xFF);

	// Set clock
	if (dat->version == VERSION_VS1053){
		writeRegister(REG_CLOCKF, 0x9800); // 3.5x with additional 2.5x
	}
	if (dat->version == VERSION_VS1063){
		writeRegister(REG_CLOCKF, 0xD000); // 4.5x with additional 1.5x
	}
	
	if (dat->version == VERSION_VS1063){
		spi_set_speed(SPI_MHZ(10)); // run at 10MHz
	}else{
		spi_set_speed(SPI_MHZ(5)); // run at 5MHz
	}
	
	timer_delay(TIMER_MILLIS(2)); // let clock and stuff settle
	
	// Now load patches
	D(DebugPrint(DEBUG_LEVEL,"resetVS1053: applying patch\n"));
	if (dat->version == VERSION_VS1053){
		loadCompressedPatch(vs1053b_patch_plugin, sizeof(vs1053b_patch_plugin));
	}
	if (dat->version == VERSION_VS1063){
		loadCompressedPatch(vs1063a_patch_plugin, sizeof(vs1063a_patch_plugin));
	}
	if (!waitDREQ_TO(dat, TRUE, 5)){ // patches cause another reset
		D(DebugPrint(ERROR_LEVEL, "resetVS1053: failed patch reset\n"));
		return FALSE ;
	}
	D(DebugPrint(DEBUG_LEVEL,"resetVS1053: patch applied\n"));
	
#if VS1053_AUDIO_ADMIX
	// Load ADMIX patch and enable
	if (dat->version == VERSION_VS1053){
		D(DebugPrint(DEBUG_LEVEL,"resetVS1053: applying admix patch\n"));
		loadCompressedPatch(vs1053b_admix_stereo_plugin, sizeof(vs1053b_admix_stereo_plugin));
		if (!waitDREQ_TO(dat, TRUE, 5)){ // patches cause another reset
			D(DebugPrint(ERROR_LEVEL, "resetVS1053: failed admix patch reset\n"));
			return FALSE ;
		}
		D(DebugPrint(DEBUG_LEVEL,"resetVS1053: admix patch applied\n"));
	}
#endif
	
	// Set modes
	newMode = SM_SDISHARE | SM_SDINEW;
#if VS1053_AUDIO_ADMIX
	newMode |= SM_LINE1; // enable line-in
#endif
#if VS1053_AUDIO_MPEG12
	newMode |= SM_LAYER12;
#endif
	writeRegister(REG_MODE, readRegister(REG_MODE) | newMode);
	
#if VS1053_AUDIO_ADMIX
	if(dat->version == VERSION_VS1053){
		// Set mix gain to -3db
		writeRegister(REG_AICTRL0, 0xFFFd);
		writeRegister(REG_AIADDR, 0x0F00);
	}
	if (dat->version == VERSION_VS1063){
		writeMemoryWord(dat, ADDR63_ADMIX_GAIN, 0xFFFd); // -3db
		writeMemoryWord(dat, ADDR63_ADMIX_CONFIG, VS63ADMIXER_RATE48 | VS63ADMIXER_MODESTEREO);
		dat->playMode |= VS63PLAYMODE_ADMIX;
	}
	dat->admix = TRUE;
#endif

	// Enable correct play mode for VS1063
	if (dat->version == VERSION_VS1063){
		initEqualiser(dat); // Set frequencies
		writeMemoryWord(dat, ADDR63_PLAYMODE, dat->playMode);
	}
	
	// Enable VU meter for VS1053
	if (dat->version == VERSION_VS1053){
		writeRegister(REG_STATUS, readRegister(REG_STATUS) | SS_VUMETER);
	}// Already set for VS1063 in PlayMode
	
	return TRUE;
}

/****** vs1053/initVS1053 *****************************************************
*
* NAME
*     initVS1053 -- Create VS1053 instance. 
*
* SYNOPSIS
*     struct VSData* = initVS1053(LONG);
* 
* FUNCTION
*     Initialise VS1053 hardware, SPIder and start background task. This must
*     be started from a process, not a task.
*
* INPUTS
*     LONG priority - -125 to 125 OS priority for worker task. Note that if the
*                     priority is higher than calling then could result in signal
*                     issues
*
* RESULT
*     struct VSData* - allocated struct for the instance. Must be freed with 
*                      destroyVS1053. Returns NULL if error occurred.
* 
* NOTES
*     Unpredictable if run on system without VS1053 hardware attached to SPIder.
*     Does not support VS1033 or other models of decoder. Versions are checked
*     and will fail initialisation if hardware is different.
*     High priorities can cause issues with signals not arriving in calling task
*     and buffers not getting filled. About 5 to reduce glitching of buffer fills
*     works. Building without interrupts (experimental) overrides this setting. 
*
* SEE ALSO
*     destroyVS1053()
*
*******************************************************************************
*
*/
__SAVE_DS__ struct VSData* initVS1053(LONG priority)
{
	struct VSData *dat = NULL ;
	BOOL success = FALSE ;
	UWORD reg = 0;
	struct IORequest *callingTmr = NULL;

#ifdef __VBCC__
	if (!SysBase){
		SysBase = *(struct ExecBase **)4UL;
	}
#endif
	
	dat = (struct VSData*)AllocVec(sizeof(struct VSData), MEMF_ANY | MEMF_CLEAR);
	if (!dat){
		return NULL;
	}
	
	_resetList(&dat->bufferList);
	_resetList(&dat->freeList);
	dat->sig = -1;
	dat->initspi = FALSE ;
	dat->panning = 50;
	dat->playMode = VS63PLAYMODE_5BAND | VS63PLAYMODE_VUMETER; // All desired defaults for VS1063
	
	if (!(callingTmr = openTimer())){
		D(DebugPrint(ERROR_LEVEL, "initVS1053: couldn't open timer\n"));
		goto close;
	}
	
	if ((dat->callingSig = AllocSignal(-1)) < 0){
		D(DebugPrint(ERROR_LEVEL, "initVS1053: couldn't allocate signal\n"));
		goto close;
	}
	
	InitSemaphore(&dat->sem);

	read_and_parse_config_file(&dat->config);
	dat->callingTask = FindTask(NULL);
	
#if VS10XX_USE_INTERRUPTS
	dat->stdPriority = priority ;
#else
	// Too instable with normal or higher priorities
	dat->stdPriority = -10;
#endif
	dat->drvTask = createWorkerTask(dat, "SPIAudio", dat->stdPriority, workerTask, 4096, dat);

	if (!timerWaitTO(callingTmr, 5, 0, 1 << dat->callingSig)){
		// Task must have failed
		D(DebugPrint(ERROR_LEVEL, "initVS1053: no task signal received to confirm worker running\n"));
		goto close;
	}

	if (!dat->drvPort){ // port is NULL if worker had any problems starting
		goto close;
	}
	
	//D(DebugPrint(DEBUG_LEVEL,"initVS1053: Completed init, REG_MODE set to 0x%04X\n", readRegister(REG_MODE)));
	
#ifdef _DEBUG
	//D(DebugPrint(DEBUG_LEVEL,"initVS1053: EndFillByte 0x%02X\n", dat->endfill));
	//D(DebugPrint(DEBUG_LEVEL,"initVS1053: REG_STATUS set to 0x%04X\n", readRegister(REG_STATUS)));
	//D(DebugPrint(DEBUG_LEVEL,"initVS1053: REG_MODE set to 0x%04X\n", readRegister(REG_MODE)));
	//D(DebugPrint(DEBUG_LEVEL,"initVS1053: REG_VOL set to 0x%04X\n", readRegister(REG_VOL)));
#endif

	success = TRUE ;
close:
	if (dat->callingSig >= 0){ // This is just temporary to sync worker task start-up
		FreeSignal(dat->callingSig);
		dat->callingSig = -1;
	}
	dat->callingTask = NULL ; // Remove reference after startup
	if (callingTmr){
		timerCloseTimer(callingTmr);
	}
	if (!success){
		if (dat){
			destroyVS1053(dat);
			dat = NULL;
		}
	}
	return dat ;
}

void destroyVS1053(struct VSData *dat)
{
	if (dat){
		if (dat->drvTask && dat->sigTerm >= 0){
			// Shutdown task
			Signal(dat->drvTask, 1 << dat->sigTerm); // Tell worker task to stop
		}else{
			// Cannot signal, so just free here
			FreeVec(dat);
		}
	}
}