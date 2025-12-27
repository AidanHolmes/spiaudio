/*
 * Written in the end of April 2020 by Niklas Ekström.
 * Updated in July 2021 by Niklas Ekström to handle Card Present signal.
 * Adapted in April 2025 by Niklas Ekström to work with the SPIder expansion.
 * Updated in December 2025 by Aidan Holmes for quick implementation using SAS/C and various addon SPI boards
 */
#ifndef SPI_H_
#define SPI_H_

#include <exec/types.h>

#define SPI_MHZ(x)				(128 + x)
#define SPI_KHZ(x)				x

#define SPI_SPEED_SLOW SPI_KHZ(40)
#define SPI_SPEED_FAST SPI_MHZ(16)

#define SPIDER_PINID(x)			(1 << (x-20))
#define PIN_CD					SPIDER_PINID(20)
#define PIN_INT					SPIDER_PINID(21)

int spi_initialize(struct ClockportConfig *config, BYTE sig); // Set sig to use when interrupts fired.
void spi_diag(void); // print state of SPI interrupts and GPIO vals

void spi_enable_interrupt(void);
void spi_disable_interrupt(void);
// Returns bit mask of pin which fired the interrupt and clears the interrupt to fire again
unsigned char spi_reset_interrupt(void);
// Get the value of pin provided into function. 1 for high and 0 for low
int spi_pin_val(unsigned char pin);
void spi_shutdown(void);
void spi_set_speed(unsigned char speed); // Set speed or use macros for FAST or SLOW
void spi_select(void); //enable SS/CS (low)
void spi_deselect(void); //disable SS/CS (high)
void __asm __saveds spi_read(register __a0 unsigned char *buf, register __d0 short size);
void __asm __saveds spi_write(register __a0 const unsigned char *buf, register __d0 short size);

#endif
