/**
 * Interface to MCP23S17 I/O Expander via SPI bus
 */

#ifndef __IOEXPANDER_H__
#define __IOEXPANDER_H__

void ioexpander_init();
void ioexpander_read_inputs(uint8_t *porta, uint8_t *portb);
void ioexpander_read_outputs(uint8_t *porta, uint8_t *portb);
void ioexpander_set_outputs(uint8_t porta, uint8_t portb);
void ioexpander_debug_inputs();
void ioexpander_debug_outputs();

extern uint8_t inputFlag;

#endif // __IOEXPANDER_H__
