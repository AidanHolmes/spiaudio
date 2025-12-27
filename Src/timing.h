#ifndef __OS_TIMING
#define __OS_TIMING

#include <proto/timer.h>
#include <devices/timer.h>
#include <exec/io.h>
#include <exec/types.h>
#include <exec/exec.h>
#include <proto/exec.h>

#ifndef TIMER_TICK_FREQ
/*! Tick frequency in Hz - defines timer resolution */
#define TIMER_TICK_FREQ			50
#endif

/*! Helper macro calculates number of ticks in specified ms (rounds up) */
#define TIMER_MILLIS(ms)		(((ULONG)(ms) * (TIMER_TICK_FREQ) + 999ul) / 1000ul)

/*! Helper macro calculates number of ticks in specified s */
#define TIMER_SECONDS(s)		((ULONG)(s) * (TIMER_TICK_FREQ))

/*! Helper macro converts ticks back to milliseconds (rounds down) */
#define TIMER_TO_MILLIS(ticks)	((ULONG)(ticks) * 1000ul / (TIMER_TICK_FREQ))

/*! Helper macro converts ticks back to seconds (rounds down) */
#define TIMER_TO_SECONDS(ticks)	((ULONG)(ticks) / (TIMER_TICK_FREQ))

/*!
 * Returns current 32-bit tick counter in
 * increments of TIMER_TICK_FREQ
 *
 * \return				Current tick count
 */
ULONG timer_get_tick_count(void);
void timer_delay(ULONG ticks);

// Setup and sendio the timer request
void setTimer(struct IORequest* tmr, ULONG secs, ULONG micro);
// wait on timer to complete or another signal to complete - returns completed signal or zero if timer expired
ULONG waitTO(struct IORequest* tmr, ULONG sigs);
// combines a setTimer and waitTO in one call
ULONG timerWaitTO(struct IORequest* tmr, ULONG secs, ULONG micro, ULONG sigs);
// Create new timer. Save the IORequest for further calls
struct IORequest *openTimer(void);
// Remove the timer and IORequest
void timerCloseTimer(struct IORequest *tmr);
// wait a very short time of 400 nano sec - doesn't use timer resource but instead calculated processing delay
void timerWait400ns(ULONG itersPer400ns);
// Run timerCalibrate before timerWait400ns. itersPer400ns should be used for timerWait400ns. 
BOOL timerCalibrate(struct IORequest* tmr, ULONG *itersPer400ns);

#endif