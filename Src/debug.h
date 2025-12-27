#ifndef DEBUG_H
#define DEBUG_H

#include <exec/types.h>

// define the macro below to get output to serial
#define NONE_LEVEL  99
#define ERROR_LEVEL 10
#define DEBUG_LEVEL 0
#define INFO_LEVEL  1
#define WARN_LEVEL  2

// Macros for debug output
#ifdef _DEBUG
#define D(x) x
//#define DebugInit(x) _DebugInit(x)
#define DebugPrint1(l,f,x1) _DebugPrint(l,f,x1)
#define DebugPrint2(l,f,x1,x2) _DebugPrint(l,f,x1,x2)
#define DebugPrint3(l,f,x1,x2,x3) _DebugPrint(l,f,x1,x2,x3)
#define DebugPrint4(l,f,x1,x2,x3,x4) _DebugPrint(l,f,x1,x2,x3,x4)
#define DebugPrint5(l,f,x1,x2,x3,x4,x5) _DebugPrint(l,f,x1,x2,x3,x4,x5)
#define DebugPrint6(l,f,x1,x2,x3,x4,x5,x6) _DebugPrint(l,f,x1,x2,x3,x4,x5,x6)
#define DebugPrint7(l,f,x1,x2,x3,x4,x5,x6,x7) _DebugPrint(l,f,x1,x2,x3,x4,x5,x6,x7)
#define DebugPrint8(l,f,x1,x2,x3,x4,x5,x6,x7,x8) _DebugPrint(l,f,x1,x2,x3,x4,x5,x6,x7,x8)
#else
#define D(x)
//#define DebugInit(x)
#define DebugPrint1(l,f,x1)
#define DebugPrint2(l,f,x1,x2)
#define DebugPrint3(l,f,x1,x2,x3)
#define DebugPrint4(l,f,x1,x2,x3,x4)
#define DebugPrint5(l,f,x1,x2,x3,x4,x5)
#define DebugPrint6(l,f,x1,x2,x3,x4,x5,x6)
#define DebugPrint7(l,f,x1,x2,x3,x4,x5,x6,x7)
#define DebugPrint8(l,f,x1,x2,x3,x4,x5,x6,x7,x8)
#endif

// Must Init before calling other functions as it sets the level
void DebugInit(int level);
void DebugPrint(int level, char *format, ...);
int DebugPutChar(register int chr);
int DebugMayGetChar(void);

void DebugPutStr(register const char *buff);
void DebugPutHex(const char *what, ULONG val);
void DebugPutDec(const char *what, ULONG val);
void DebugPutHexVal(ULONG val);


#endif /* DEBUG_H */
