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

void send_inputs();
void send_outputs();
void process_usb();
void process_command();
void readTemp();

char online = FALSE;
char usbInBuf[32];
unsigned char usbPtr = 0;
//unsigned char readTempFlag = 0;

int main()
{
	CLKPR = B(CLKPCE); CLKPR = B(CLKPS2);  // set for 16/16=1 MHz clock
	set_sleep_mode(SLEEP_MODE_IDLE);  // just idle for sleep mode so we can receive USB messages
	ACSR |= B(ACD); // disable Analog comparator
	WDTCSR = 0; // Watchdog off
	PRR0 = B(PRTWI)  | B(PRTIM2) | B(PRTIM1) | B(PRTIM0) | B(PRADC); // disable timers[0,1,2], twi and ADC
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
	_delay_ms(100);
	ioexpander_init();
	//ds1820_search_bus();
	
	while (1) 
	{
		if (usbFlag)
		{
			usbFlag = 0;
			process_usb();
		}

		if (inputFlag)
		{
			inputFlag = 0;
			send_inputs();
		}

		/*
		if (readTempFlag)
		{
			readTempFlag = 0;
			ds1820_read_temps();
		}
		*/

		sleep_mode(); // until next interrupt
	}
}

/*  Not using the one-wire temps anymore, just keeping for reference
void tempTimer()
{
	// 4Mhz/1024prescale = 3906Hz
	// This causes COMPA about 1 second after we start the timer, only need 750ms
	TCNT1H = 0;
	TCNT1L = 0;
	OCR1A = 3900;
	TIMSK1 = B(OCIE1A);
	TCCR1A = 0;
	TCCR1B = B(CS12) | B(CS10);	
}

ISR(TIMER1_COMPA_vect)
{
	readTempFlag = 1; // let main know to call readTemp
	TCCR1B = 0; // stop the timer
}
*/

void send_inputs()
{
	uint8_t porta, portb;
	ioexpander_read_inputs(&porta, &portb);
	usb_printf_P(PSTR("I=%X,%X\r\n"), porta, portb);
}

void send_outputs()
{
	uint8_t porta, portb;
	ioexpander_read_outputs(&porta, &portb);
	usb_printf_P(PSTR("O=%X,%X\r\n"), porta, portb);
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
	uint8_t a, b;

	if (usbPtr < 1)
	{
		usbPtr = 0;
		return;
	}

	usbInBuf[usbPtr] = 0;
	switch (usbInBuf[0])
	{
		/*
		case 'T':
			ds1820_start_temps();
			tempTimer();
			break;
		case 'S':
			ds1820_search_bus();
			break;
		*/
		case 'X':
			ioexpander_debug_inputs();
			break;
		case 'Y':
			ioexpander_debug_outputs();
			break;
		case 'I': 
			send_inputs();
			break;
		case 'O':
			if (usbPtr < 6)
			{
				usb_printf_P(PSTR("Error in Output\r\n"));
				break;
			}
			sscanf_P(usbInBuf, PSTR("O=%hhX,%hhX"), &a, &b);
			ioexpander_set_outputs(a, b);
			send_outputs();
			break;
	}

	usbPtr = 0;
	usb_flush_output();
}

