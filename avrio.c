/**
 * This is the primary alarm logic
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include "ports.h"
#include "usbserial.h"
#include "ioexpander.h"

void filtered_read();
void sample_inputs();
void process_usb();
void process_command();
void readTemp();

#define DEBOUNCE_ARRAY_LENGTH 10

uint8_t online = FALSE;
uint8_t usbPtr = 0;
char usbInBuf[32];

uint16_t outputs = 0;
uint8_t  sampleFlag = FALSE; 
uint8_t  debounceidx = 0;
uint16_t debounceSamples[DEBOUNCE_ARRAY_LENGTH];
uint16_t SAMPLE_TIMER = 2000; // defaults to about 2ms between reads (2000 ticks @ 1Mhz)

int main()
{
	CLKPR = B(CLKPCE); CLKPR = B(CLKPS2);  // set for 16/16=1 MHz clock  (NOTE: 1-Wire bus timing only worked when at 4Mhz, regardless of F_CPU macro setting)
	set_sleep_mode(SLEEP_MODE_IDLE);  // just idle for sleep mode so we can receive USB messages
	ACSR |= B(ACD); // disable Analog comparator
	WDTCSR = 0; // Watchdog off
	PRR0 = B(PRTWI)  | B(PRTIM2) | B(PRTIM0) | B(PRADC); // disable timers[0,2], twi and ADC
	PRR1 = B(PRTIM3) | B(PRUSART1); // disable timer3 and usart

	/* Set MOSI, SCK, SS output, all others input */
	DDRB = B(MOSI) | B(SCLK) | B(SS);

	/* INTs are input, no pullups */
	//PORTD
	//DDRD

	/* CSs are output, outputs high */
	CSPORT = CSINPUT | CSOUTPUT;
	CSDDR  = CSINPUT | CSOUTPUT;

	/* external INTs 2 on falling edge (pins 2) */  
	EICRA = B(ISC21);
	EIMSK = B(INT2);

	/* Enable SPI, Master, set clock rate fck/16 (same as CPU clk), no int */
	SPCR = B(SPE) | B(MSTR) | B(SPR0);

	usb_init();
	sei();
	while (!usb_configured());
	ioexpander_init();
	_delay_ms(100);
	
	while (1) 
	{
		if (usbFlag)
		{
			usbFlag = 0;
			process_usb();
		}

		if (ioexpander_inputFlag)
		{
			ioexpander_inputFlag = FALSE;
			filtered_read();
		}
		
		if (sampleFlag)
		{
			sample_inputs();
		}

		sleep_mode(); // until next interrupt
	}
}


// Start our timer to interrupt at about 1KHz, add variation to our samples to make sure we sample a full range
void filtered_read()
{
	TCNT1H = 0;
	TCNT1L = 0;
	OCR1A  = SAMPLE_TIMER;  // interrupt at set timer rate
	TIMSK1 = B(OCIE1A);
	TCCR1A = 0;
	TCCR1B = B(WGM12) | B(CS10); // count to OCR1A and reset, clk=cpu
	
	debounceidx = 0;
	for (uint8_t ii = 0; ii < DEBOUNCE_ARRAY_LENGTH; ii++) { debounceSamples[ii] = 1 << ii; }
	sampleFlag = TRUE;  // start one now
}

// When the counter goes off, start another input sample
ISR(TIMER1_COMPA_vect)
{
	sampleFlag = TRUE;
}

// Sample all the inputs and wait until everything settles
void sample_inputs()
{	
	ioexpander_read_inputs(&(debounceSamples[debounceidx]));
	debounceidx = (debounceidx + 1) % DEBOUNCE_ARRAY_LENGTH;

	uint16_t ands = 0xFFFF;
	uint16_t ors  = 0;
	for (uint8_t ii = 0; ii < DEBOUNCE_ARRAY_LENGTH; ii++)
	{
		ands &= debounceSamples[ii];
		ors  |= debounceSamples[ii];
	}
		
	if (ands == ors)
	{
		TCCR1B = 0; // stop the timer and further sampling
		usb_printf_P(PSTR("I=%hX\r\n"), ands);
	}
	
	sampleFlag = FALSE;
}

void process_usb()
{
	int16_t curc;

	if (!online && usb_configured() && (usb_get_control() & USB_SERIAL_DTR))
	{
		online = TRUE;
		usb_flush_output();
	}

	if (!online)
		return;

	if (usb_available() == 0)
		return;

	while (TRUE)
	{
		curc = usb_getchar();
		if (curc != -1) 
		{
			usbInBuf[usbPtr++] = curc;
			if ((curc == '\r') || (curc == '\n'))
				process_command();

			if (usbPtr >= sizeof(usbInBuf))
				usbPtr = 0; // buffer overflow??

			continue;
		} 

		if (!usb_configured() || !(usb_get_control() & USB_SERIAL_DTR))
		{
			online = FALSE;
			usbPtr = 0;
		}

		return;
	}
}


void process_command()
{
	uint16_t bitmask, timertemp;
	uint8_t port, val;

	if (usbPtr < 1)
	{
		usbPtr = 0;
		return;
	}

	usbInBuf[usbPtr] = 0;
	switch (usbInBuf[0])
	{
		case 'S':
			if (usbPtr >= 6) 
			{
				sscanf_P(usbInBuf, PSTR("S=%hX"), &timertemp);
				if (timertemp > 100)  // minor validation
					SAMPLE_TIMER = timertemp;
			}
			usb_printf_P(PSTR("S=%hX\r\n"), SAMPLE_TIMER);
			break;
			
		case 'I': 
			filtered_read();
			break;
			
		case 'O':
			if (usbPtr >= 5)  // if we have actual data to set, parse it and set it now
			{
				sscanf_P(usbInBuf, PSTR("O%hhX=%hhX"), &port, &val);
				bitmask = 1 << port;
				if (val)
					outputs |= bitmask;
				else
					outputs &= ~bitmask;
				ioexpander_set_outputs(outputs);
			}
			
			// reread our outputs so everyone is in sync
			ioexpander_read_outputs(&outputs);
			usb_printf_P(PSTR("O=%hX\r\n"), outputs);
			break;
	}

	usbPtr = 0;
	usb_flush_output();
}

