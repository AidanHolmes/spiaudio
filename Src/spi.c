/*
 * Written in the end of April 2020 by Niklas Ekström.
 * Updated in July 2021 by Niklas Ekström to handle Card Present signal.
 * Adapted in April 2025 by Niklas Ekström to work with the SPIder expansion.
 */
#include <exec/types.h>
#include <exec/interrupts.h>
#include <exec/libraries.h>

#include <hardware/intbits.h>

#include <proto/exec.h>
#include <string.h>

#include "spi.h"
#include "config_file.h"
#include "debug.h"
#include "timing.h"

#define REG_STATUS          	0	// RO
#define REG_RESERVED_1          1
#define REG_UPPER_LENGTH        2   // WO, Upper byte for lengths
#define REG_GPIOS 		        3   // RO, Read GPIO values 20-27
#define REG_RX_HEAD             4   // RO
#define REG_RX_TAIL             5   // RO
#define REG_TX_HEAD             6   // RO
#define REG_TX_TAIL             7   // RO
#define REG_RX_DISCARD          8   // WO, Lower byte
#define REG_TX_FEED             9   // WO, Lower byte
#define REG_SPI_FREQ            10  // WO, Set SPI frequency
#define REG_SLAVE_SELECT        11  // WO, Write SS
#define REG_INT_FIRED           12  // RW
#define REG_INT_ARMED           13  // WO
#define REG_FIFO                14  // RW, Write to TX, Read from RX
#define REG_IDENT               15  // RO

#define IRQ_CD_CHANGED          PIN_CD
#define IRQ_EXINT_CHANGED       PIN_INT

#define STATUS_RX_DISCARD_EMPTY 0x01

#define TEN_KHZ                 10000
#define ONE_MHZ                 1000000

#define IDENT_SIZE              8

typedef void (*VOID_FUNC)();

static const UBYTE ident_str[] = {0xff, 's', 'p', 'd', 'r'};

#define DEFAULT_CLOCKPORT_ADDRESS   0xD80001
#define DEFAULT_INTERRUPT_NUMBER    6

static struct ClockportConfig clockport_config = {DEFAULT_CLOCKPORT_ADDRESS,DEFAULT_INTERRUPT_NUMBER};

static volatile BYTE *clockport_address;

static unsigned char speedMode = SPI_SPEED_SLOW;

#define CP_REG(reg)         ((volatile UBYTE *)(clockport_address + ((reg) << 2)))
#define CP_WR(reg, val)     (*CP_REG((reg)) = (val))
#define CP_RD(reg)          (*CP_REG((reg)))

static const char spi_lib_name[] = "spi-lib-spider";

struct InterruptData
{
    volatile UBYTE *clockport_address;
	struct Task *task;
	BYTE sig;
	UBYTE lastINT;
};

// 26-Aug-25 Aidan Holmes change to implement in C code and trigger signal
static void __interrupt __saveds __asm SPI_Interrupt(register __a1 struct InterruptData* dat, register __a6 APTR _card_Code)
{
	// DO NOT PRINT TO STDOUT IN INTERRUPT - SERIAL IS OK
	// Capture what fired then reset interrupt immediately
	dat->lastINT = CP_RD(REG_INT_FIRED);
	CP_WR(REG_INT_FIRED, 0);
	
	Signal(dat->task, 1 << dat->sig);
}

static struct InterruptData interrupt_data;
static struct Interrupt ports_interrupt;

void spi_diag(void)
{
	UBYTE intmask = CP_RD(REG_GPIOS);
	D(DebugPrint(DEBUG_LEVEL, "REG_INT_FIRED 0x%02X, EXTERNAL INT %d, CARD DETECT %d\n", CP_RD(REG_INT_FIRED), (intmask & PIN_INT)?1:0, (intmask & PIN_CD)?1:0));
}

__inline void spi_select(void)
{
    CP_WR(REG_SLAVE_SELECT, 1);
	//timer_delay(TIMER_MILLIS(10));
}

__inline void spi_deselect(void)
{
	if (speedMode == SPI_SPEED_SLOW){
		timer_delay(TIMER_MILLIS(10));
	}
    CP_WR(REG_SLAVE_SELECT, 0);
}

__inline int spi_pin_val(unsigned char pin)
{
	return (CP_RD(REG_GPIOS) & pin)?1:0;
}

__inline void spi_enable_interrupt(void)
{
	CP_WR(REG_INT_ARMED, 0xFF);
}

__inline void spi_disable_interrupt(void)
{
	CP_WR(REG_INT_ARMED, 0);
}

__inline unsigned char spi_reset_interrupt(void)
{	
	D(DebugPrint(DEBUG_LEVEL,"spi_reset_interrupt: interrupt val 0x%02X\n", interrupt_data.lastINT));

    // Re-enable the CD changed interrupt.
	//CP_WR(REG_INT_ARMED, IRQ_EXINT_CHANGED | IRQ_CD_CHANGED);
	
	CP_WR(REG_INT_FIRED, 0);

    return interrupt_data.lastINT;
}

void spi_set_speed(unsigned char speed)
{
    //UBYTE freq = speed == SPI_SPEED_FAST ? (128 + 16) : 40;
	speedMode = speed ;
    CP_WR(REG_SPI_FREQ, speed);
}

// These two assembly functions were contributed by Patrik Axelsson.
extern void __asm copy_from_reg(register __a0 UBYTE *dst, register __a1 volatile UBYTE *reg, register __d0 WORD length);
extern void __asm copy_to_reg(register __a0 volatile UBYTE *reg, register __a1 const UBYTE *src, register __d0 WORD length);

void __asm copy_from_reg2(register __a0 UBYTE *dst, register __a1 volatile UBYTE *reg, register __d0 WORD length)
{
	WORD i = 0;
	for (;i < length;i++){
		*dst++ = *reg;
	}
}

void __asm copy_to_reg2(register __a0 volatile UBYTE *reg, register __a1 const UBYTE *src, register __d0 WORD length)
{
	WORD i = 0;
	for (;i < length;i++){
		*reg = *src++;
	}
}


void  __asm __saveds spi_read(register __a0 UBYTE *buf, register __d0 WORD size)
{
	volatile UBYTE *fifo = NULL;
	UBYTE rx_head =0, bytes_in_rx =0, rx_tail =0;
	UWORD retry = 1000;

    CP_WR(REG_UPPER_LENGTH, size >> 8);
    CP_WR(REG_TX_FEED, size & 0xff);

    fifo = CP_REG(REG_FIFO);

    rx_head = CP_RD(REG_RX_HEAD);

    if (size == 1)
    {
        do
        {
            rx_tail = CP_RD(REG_RX_TAIL);
        }
        while (rx_head == rx_tail);

        *buf = *fifo;
    }
    else
    {
        do
        {
            rx_tail = CP_RD(REG_RX_TAIL);

            bytes_in_rx = rx_tail - rx_head;
			
			//D(DebugPrint(DEBUG_LEVEL,"spi_read: Bytes in RX %u, head %u, tail %u, remaining to read %u\n", bytes_in_rx, rx_head, rx_tail, size));

            if (bytes_in_rx)
            {
                copy_from_reg(buf, fifo, bytes_in_rx);
                buf += bytes_in_rx;
                rx_head += bytes_in_rx;
                size -= bytes_in_rx;
            }
			//if (--retry == 0){
			//	D(DebugPrint(DEBUG_LEVEL,"spi_read: Failed! - Bytes in RX %u, head %u, tail %u, remaining to read %u\n", bytes_in_rx, rx_head, rx_tail, size));
			//	break;
			//}
        }
        while (size);
    }
}

void  __asm __saveds spi_write(register __a0 const UBYTE *buf, register __d0 WORD size)
{
	volatile BYTE *fifo = NULL;
	UBYTE tx_head =0, tx_tail =0, next_tx_tail=0, bytes_in_tx =0, free_space =0;
	UWORD retry = 1000;
	
    CP_WR(REG_UPPER_LENGTH, size >> 8);
    CP_WR(REG_RX_DISCARD, size & 0xff);

    fifo = CP_REG(REG_FIFO);

    tx_tail = CP_RD(REG_TX_TAIL);

    if (size == 1){
        next_tx_tail = tx_tail + 1;
        do{
            tx_head = CP_RD(REG_TX_HEAD);
        }while (next_tx_tail == tx_head);

        *fifo = *buf;
    }else{
        do{
            tx_head = CP_RD(REG_TX_HEAD);

            bytes_in_tx = tx_tail - tx_head;
            free_space = 255 - bytes_in_tx;
			
			//D(DebugPrint(DEBUG_LEVEL,"spi_read: Bytes free in TX %u, head %u, tail %u, remaining to write %u\n", free_space, tx_head, tx_tail, size));

            if (free_space){
                if (free_space > size){
                    free_space = size;
				}

                copy_to_reg(fifo, buf, free_space);
                buf += free_space;
                tx_tail += free_space;
                size -= free_space;
            }
			//if (--retry == 0){
			//	D(DebugPrint(DEBUG_LEVEL,"spi_read: Failed! - Bytes free in TX %u, head %u, tail %u, remaining to write %u\n", free_space, tx_head, tx_tail, size));
			//	break;
			//}
        }while (size);
    }
	while((CP_RD(REG_STATUS) & STATUS_RX_DISCARD_EMPTY) == 0){
//		if (--retry == 0){
//			D(DebugPrint(DEBUG_LEVEL,"spi_read: Failed! - Status 0x%02X\n", CP_RD(REG_STATUS)));
//			break;
//		}
	}
}

static int probe_interface(void)
{
    UBYTE read_bytes[IDENT_SIZE], fw_major_ver=0, fw_minor_ver=0, fw_patch_ver=0;
	BOOL found = FALSE;
    short pos = 0, i=0, start = 0;

    for (; i < IDENT_SIZE; i++){
        read_bytes[i] = CP_RD(REG_IDENT);
	}

    for (; start < IDENT_SIZE && !found; start++)
    {
        found = TRUE;
        pos = start;

        for (i = 0; i < sizeof(ident_str); i++)
        {
            if (read_bytes[pos] != ident_str[i])
            {
                found = FALSE;
                break;
            }
            pos = (pos + 1) & (IDENT_SIZE - 1);
        }
    }

    if (!found){
        D(DebugPrint(ERROR_LEVEL,"SPIder: probe failed\n"));
		return -1;
	}

    fw_major_ver = read_bytes[pos];

    // Only major version 1 is currently supported.
    if (fw_major_ver != 1)
        return -2;

    pos = (pos + 1) & (IDENT_SIZE - 1);
    fw_minor_ver = read_bytes[pos];

    pos = (pos + 1) & (IDENT_SIZE - 1);
    fw_patch_ver = read_bytes[pos];

	D(DebugPrint(DEBUG_LEVEL,"SPIder firmware version: %ld.%ld.%ld\n", (ULONG)fw_major_ver, (ULONG)fw_minor_ver, (ULONG)fw_patch_ver));

    return 0;
}

int spi_initialize(struct ClockportConfig *config, BYTE sig)
{
	LONG int_num = 0;

    clockport_address = (volatile UBYTE *)config->clockport_address;
	
	D(DebugPrint(DEBUG_LEVEL,"SPIder on clockport: %p\n", clockport_address));

    if (probe_interface() < 0)
        return -1;

	spi_set_speed(SPI_SPEED_SLOW);

    CP_WR(REG_INT_ARMED, 0xFF); // Arm all - TO DO: add some control for different interrupts on pins (via device tree)
    CP_WR(REG_INT_FIRED, 0);
	
    interrupt_data.clockport_address = clockport_address;
	interrupt_data.sig = sig;
	interrupt_data.task = FindTask(NULL);
	interrupt_data.lastINT = 0;
	
	memset(&ports_interrupt, 0, sizeof(struct Interrupt));

    ports_interrupt.is_Node.ln_Type = NT_INTERRUPT;
    ports_interrupt.is_Node.ln_Pri = -60;
    ports_interrupt.is_Node.ln_Name = (char *)spi_lib_name;
    ports_interrupt.is_Data = (APTR)&interrupt_data;
    ports_interrupt.is_Code = (VOID_FUNC)SPI_Interrupt;

    int_num = config->interrupt_number == 2 ? INTB_PORTS : (config->interrupt_number == 3 ? INTB_VERTB : INTB_EXTER);
	AddIntServer(int_num, &ports_interrupt);

    return 0;
}

void spi_shutdown(void)
{
	LONG int_num = 0;
	
    CP_WR(REG_INT_ARMED, 0);
    CP_WR(REG_INT_FIRED, 0);

    int_num = clockport_config.interrupt_number == 2 ? INTB_PORTS : (clockport_config.interrupt_number == 3 ? INTB_VERTB : INTB_EXTER);
	if (ports_interrupt.is_Data){ // Check one of the attributes is set, indicating the interrupt exists
		RemIntServer(int_num, &ports_interrupt);
	}
}
