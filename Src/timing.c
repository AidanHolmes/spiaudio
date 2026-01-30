#include "timing.h"
#include "debug.h"
#include "compatibility.h"

static volatile UBYTE * const todl = (volatile UBYTE*)0xbfe801;
static volatile UBYTE * const todm = (volatile UBYTE*)0xbfe901;
static volatile UBYTE * const todh = (volatile UBYTE*)0xbfea01;

ULONG timer_get_tick_count(void)
{
	UBYTE l,m,h;

	/* TOD registers latch on reading MSB, unlatch on reading LSB */
	h = *todh;
	m = *todm;
	l = *todl;
	return ((ULONG)h << 16) | ((ULONG)m << 8) | (ULONG)l;
}

void timer_delay(ULONG ticks)
{
	ULONG timeout = timer_get_tick_count() + ticks;
	while ((LONG)(timer_get_tick_count() - timeout) < 0) {

	}
}

BOOL timerCalibrate(struct IORequest* tmr, ULONG *itersPer400ns)
{
    register ULONG x;
    register ULONG scale = 0x8000;      // min iterations...
    register volatile ULONG t = 1;
    struct timeval t1, t2;
    struct Device *TimerBase = tmr->io_Device;
	
	*itersPer400ns = 0 ;
    
    while (scale < 0x80000000){
        Forbid();
        GetSysTime(&t1);
        for (x = 1; x < scale; x++){
            t = (((t + x) * t) - x) / x;    // add, mul, sub, div, trivial benchmark.
			//hwDelay() ;
		}

        GetSysTime(&t2);
        Permit();
        SubTime(&t2, &t1);
		D(DebugPrint(DEBUG_LEVEL,"SubTime on calibration: sec = %u, micro = %u, scale = 0X%04X\n", t2.tv_secs, t2.tv_micro, scale)) ;
        
        // Cannot be over 1 second, this will not work - slow system
        if (t2.tv_secs > 0){
			*itersPer400ns = 1 ;
            return FALSE;
        }

        /*
         * we expect at least 10000 times longer period, which should be 'achievable'
         * unlikely we will cross the magic boundary here of 4 billion instructions in 10 millisecond (yielding 400'000MIPS?)
         * on the other side, if we go as low as 1, then 4 iterations of add/sub/mul/div is perfectly fine yielding a bit more than 400ns...
         */

        if (t2.tv_micro >= 10000) // about 10 ms resolution to run the calculation
            break;
        scale <<= 1;
    }
	
	if (scale == 0x80000000 || t2.tv_micro == 0){
		// struggling to find a suitable scale, could be some CPU optimisations going on
		*itersPer400ns = 0x8000; // cannot determine scale (assume a largish default number of iterations)
	}else{	
		*itersPer400ns = 400 / (t2.tv_micro / (scale/1000));
		*itersPer400ns += 1 ; // round up
	}
    return TRUE;
}

__INLINE__ void timerWait400ns(ULONG itersPer400ns)
{
    register volatile ULONG t = 1;
	ULONG ns = itersPer400ns;
    while (ns > 0){
        t = (((t + ns) * t) - ns) / ns;    // add, mul, sub, div, trivial benchmark.
        --ns;
    }
}

__INLINE__ void setTimer(struct IORequest* tmr, ULONG secs, ULONG micro)
{
	// Stop old timers
	if (!CheckIO(tmr) && tmr->io_Command == TR_ADDREQUEST){
		AbortIO(tmr);
		WaitIO(tmr);
	}
	tmr->io_Command = TR_ADDREQUEST;
    ((struct timerequest*)tmr)->tr_time.tv_secs = secs;
    ((struct timerequest*)tmr)->tr_time.tv_micro = micro;
	
    SendIO(tmr);
}

__INLINE__ ULONG waitTO(struct IORequest* tmr, ULONG sigs)
{
	ULONG sig = 1 << tmr->io_Message.mn_ReplyPort->mp_SigBit;
	sigs = Wait(sigs | sig);
    if ((sigs & sig) == 0){
        if (!CheckIO(tmr)){
            AbortIO(tmr);
		}
    }
    WaitIO(tmr);

    SetSignal(0, sig);

    return sigs & ~sig;
}

__INLINE__ ULONG timerWaitTO(struct IORequest* tmr, ULONG secs, ULONG micro, ULONG sigs)
{
	setTimer(tmr, secs, micro);
    return waitTO(tmr,sigs);
}

struct IORequest *openTimer(void)
{
    struct MsgPort *p = CreateMsgPort();
    if (NULL != p)
    {
        struct IORequest *io = CreateIORequest(p, sizeof(struct timerequest));

        if (NULL != io)
        {
            if (0 == OpenDevice("timer.device", UNIT_MICROHZ, io, 0))
            {
                return io;
            }
            else
            {
                D(DebugPrint(ERROR_LEVEL,"Failed to open MICROHZ timer.device\n"));
            }
            DeleteIORequest(io);
        }
        else
        {
            D(DebugPrint(ERROR_LEVEL,"Failed to create timerequest\n"));
        }
        DeleteMsgPort(p);
    }
    else
    {
        D(DebugPrint(ERROR_LEVEL,"Failed to create timer port\n"));
    }

    return NULL;
}

void timerCloseTimer(struct IORequest *tmr)
{
    if (NULL != tmr)
    {
        struct MsgPort *p = tmr->io_Message.mn_ReplyPort;
        CloseDevice(tmr);
        DeleteIORequest(tmr);
        DeleteMsgPort(p);
    }
}