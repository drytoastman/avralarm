#ifndef _PORTS_H
#define _PORTS_H

/* Some system wide definitions that don't appear in AVR gcc stuff */
#define TRUE  1
#define FALSE 0
#define B(bit) (1 << (bit))

#define SPI_WAIT() 	  while(!(SPSR & (1<<SPIF)));
#define SPI_WRITE(b)  SPDR = b; SPI_WAIT();
#define SPI_READ(b)   SPDR = 0; SPI_WAIT(); *b = SPDR;

/* Port/Pin usage is defined here for use by all modules */

// Port B SPI pins
#define MISO 3
#define MOSI 2
#define SCLK 1
#define SS   0

// Port D external ints
#define INT_SPARE  1
#define INT_OUTPUT 2
#define INT_INPUT  3

// The interrupts we use
#define VEC_INPUT_EXPANDER INT2_vect

// Port F pins (io chip select and ow bus)
#define CSPORT    PORTF
#define CSPIN     PINF
#define CSDDR     DDRF
#define OWPORT    PORTF
#define OWPIN     PINF
#define OWDDR     DDRF

//#define CSSPARE   0
#define CSINPUT   B(1)
#define CSOUTPUT  B(2)
#define OWBUS1    5
#define OWBUS2    6
#define OWBUS3    7

#endif // _PORTS_H
