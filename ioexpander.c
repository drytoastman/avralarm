/**
 * Interface to MCP23S17 I/O Expander via SPI bus
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "ioexpander.h"
#include "usbserial.h"
#include "ports.h"

#define EXPANDER_READ  0x41
#define EXPANDER_WRITE 0x40

enum CONTROL_REG {
	IODIRA = 0, IODIRB, IPOLA, IPOLB, GPINTENA, GPINTENB,
	DEFVALA, DEFVALB, INTCONA, INTCONB, IOCON, IOCONDBL,
	GPPUA, GPPUB,
	INTFA, INTFB, INTCAPA, INTCAPB, GPIOA, GPIOB, OLATA, OLATB
};

unsigned char input_config[] = {
	EXPANDER_WRITE, 0x00,
	0x7F, 0x7F, 0x00, 0x00, 0x7F, 0x7F, // input for pins 0-6, regular polarity, interrupts
	0x00, 0x00, 0x00, 0x00, 0x40, 0x40, // int on change from prev state, mirror int pins
	0x7F, 0x7F  // pullups for input pins
};

unsigned char output_config[] = {
	EXPANDER_WRITE, 0x00,
	0x00, 0x00, // output for all pins
};


inline void _ioexpander_write_initbuf(uint8_t cs, uint8_t *buf, uint8_t len)
{
	CSPORT &= ~cs;
	for (uint8_t ii = 0; ii < len; ii++) {
		SPI_WRITE(buf[ii]);
	}
	CSPORT |= cs;
}

inline void _ioexpander_read_ports(uint8_t cs, uint8_t *porta, uint8_t *portb)
{
	CSPORT &= ~cs;
	SPI_WRITE(EXPANDER_READ);
	SPI_WRITE(GPIOA);
	SPI_READ(porta);
	SPI_READ(portb);
	CSPORT |= cs;
}

inline void _ioexpander_set_ports(uint8_t cs, uint8_t porta, uint8_t portb)
{
	CSPORT &= ~cs;
	SPI_WRITE(EXPANDER_WRITE);
	SPI_WRITE(GPIOA);
	SPI_WRITE(porta);
	SPI_WRITE(portb);
	CSPORT |= cs;
}

inline void _ioexpander_dump_registers(uint8_t cs)
{
	uint8_t val;
	uint8_t *valptr = &val;
	CSPORT &= ~cs;
	SPI_WRITE(EXPANDER_READ);
	SPI_WRITE(IODIRA);
	for (uint8_t ii = 0; ii <= OLATB; ii++) {
		SPI_READ(valptr);
		usb_printf_P(PSTR("0x%X = %X\r\n"), ii, val);
	}
	CSPORT |= cs;
}

void ioexpander_init()
{
	_ioexpander_write_initbuf(CSINPUT,  input_config,  sizeof(input_config));
	_ioexpander_write_initbuf(CSOUTPUT, output_config, sizeof(output_config));
}

void ioexpander_read_inputs(uint16_t *pins)
{
	uint8_t *ports = (uint8_t*)pins;
	_ioexpander_read_ports(CSINPUT, &ports[0], &ports[1]);
}

void ioexpander_read_outputs(uint16_t *pins)
{
	uint8_t *ports = (uint8_t*)pins;
	_ioexpander_read_ports(CSOUTPUT, &ports[0], &ports[1]);
}

void ioexpander_set_outputs(uint16_t pins)
{
	uint8_t *ports = (uint8_t*)&pins;
	_ioexpander_set_ports(CSOUTPUT, ports[0], ports[1]);
}

void ioexpander_debug_inputs()
{
	_ioexpander_dump_registers(CSINPUT);
}

void ioexpander_debug_outputs()
{
	_ioexpander_dump_registers(CSOUTPUT);
}


/* Input expander */
unsigned char ioexpander_inputFlag = 0;
ISR(VEC_INPUT_EXPANDER)
{
	ioexpander_inputFlag = 1;
}

