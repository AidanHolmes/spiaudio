#include <hardware/intbits.h>
#include "amiga_hwreg.h"
#include "debug.h"
#include <stdarg.h>
#include <stdio.h>


#ifndef _DEBUG
void DebugPrint(int level, char *format, ...){}
void DebugInit(int level){}
#else

// TO DO: rethink this for devices as stdio could cause crashes and therefore should not use
int g__debug_level ;

void DebugPrint(int level, char *format, ...)
{
#ifdef DEBUG_SERIAL
	char serial_buf[1024];
#endif
	va_list args;
    va_start(args, format);

    if(level >= g__debug_level){
#ifdef DEBUG_SERIAL
		vsprintf(serial_buf, format, args);
		serial_buf[1023] = '\0'; // shits gone wrong anyway but terminate an overflowed buffer
		DebugPutStr(serial_buf) ;
#else
		vprintf(format, args);
#endif
	}

    va_end(args);
}

void DebugInit(int level)
{
#ifdef DEBUG_SERIAL
	/* Set DTR, RTS, etc */
	volatile UBYTE *ciab_pra = (APTR)0xBFD000;
	volatile UBYTE *ciab_ddra = (APTR)0xBFD200;
	*ciab_ddra = 0xc0;  /* Only DTR and RTS are driven as outputs */
	*ciab_pra = 0;      /* Turn on DTR and RTS */

	/* Set the debug UART to 9600 */
	reg_w(SERPER, SERPER_BAUD(SERPER_BASE_PAL, 9600));
#endif
	g__debug_level = level ;
}

int DebugPutChar(register int chr)
{
	if (chr == '\n')
		DebugPutChar('\r');

	while ((reg_r(SERDATR) & SERDATR_TBE) == 0);
	reg_w(INTREQ, INTF_TBE);

	/* Output a char to the debug UART */
	reg_w(SERDAT, SERDAT_STP8 | SERDAT_DB8(chr));

	return 1;
}

int DebugMayGetChar(void)
{
	int c;

	if ((reg_r(SERDATR) & SERDATR_RBF) == 0)
		return -1;

	c = SERDATR_DB8_of(reg_r(SERDATR));

	/* Clear RBF */
	reg_w(INTREQ, INTF_RBF);

	return c;
}

void DebugPutStr(register const char *buff)
{
	for (; *buff != 0; buff++)
		DebugPutChar(*buff);
}

void DebugPutDec(const char *what, ULONG val)
{
	int i, num;
	DebugPutStr(what);
	DebugPutStr(": ");
	if (val == 0) {
		DebugPutChar('0');
		DebugPutChar('\n');
		return;
	}

	for (i = 1000000000; i > 0; i /= 10) {
		if (val == 0) {
			DebugPutChar('0');
			continue;
		}

		num = val / i;
		if (num == 0)
			continue;

		DebugPutChar("0123456789"[num]);
		val -= num * i;
	}
	DebugPutChar('\n');
}

void DebugPutHexVal(ULONG val)
{
	int i;
	for (i = 0; i < 8; i ++) {
			DebugPutChar("0123456789abcdef"[(val >> (28 - (i * 4))) & 0xf]);
	}
	DebugPutChar(' ');
}

void DebugPutHex(const char *what, ULONG val)
{
	DebugPutStr(what);
	DebugPutStr(": ");
	DebugPutHexVal(val);
	DebugPutChar('\n');
}

#endif