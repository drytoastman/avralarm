/************************************************************************
 * Again, this started as example code from the Internet, simple search
 * of the one-wire bus to find and use DS1820 temp sensors
 ************************************************************************/
#include <string.h>
#include <avr/pgmspace.h>
#include "owi.h"
#include "usbserial.h"
#include "ports.h"

#define	MAX_DEVICES	             16
#define DS1820_FAMILY_ID         0x10
#define DS1820_START_CONVERSION  0x44
#define DS1820_READ_SCRATCHPAD   0xbe

typedef struct {
	unsigned char bus;
	unsigned char id[8];
} Device;
Device devices[MAX_DEVICES];
unsigned char numDevices;

char _ds1820_search_busi(unsigned char bus)
{
	unsigned char ii;
	unsigned char lastDeviation;
	//unsigned char *c;

	if (numDevices >= MAX_DEVICES)
		return 1;
		
	lastDeviation = 0;
	while (1)
	{
		//usb_printf_P(PSTR("Search(%d, %X)\r\n"), lastDeviation, bus);
		if (!DetectPresence(bus))
		{
			//usb_printf_P(PSTR("SearchFailed, no presence\r\n"));
			break;
		}

		lastDeviation = SearchRom(devices[numDevices].id, lastDeviation, bus);
		if (lastDeviation > 64)
		{
			//usb_printf_P(PSTR("lastDev=%d\r\n"), lastDeviation);
			break;
		}
		
		devices[numDevices].bus = bus;
		//c = devices[numDevices].id;
		//usb_printf_P(PSTR("%d=%X/%02X%02X%02X%02X%02X%02X%02X%02X\r\n"), numDevices, devices[numDevices].bus, c[7], c[6], c[5], c[4], c[3], c[2], c[1], c[0]);
		numDevices++;
		
		if (numDevices >= MAX_DEVICES)
		break;

		// copy forward for next search
		memcpy(devices[numDevices].id, devices[numDevices-1].id, 8);

		if (lastDeviation <= 0)
		break;
	}

	// Go through all the devices and do CRC check.
	for (ii = 0; ii < numDevices; ii++)
	{
		if(CheckRomCRC(devices[ii].id))
		return 1;
	}

	return 0;
}


void ds_1820_search_bus()
{
	//usb_printf_P(PSTR("Reset and Search Bus\r\n"));
	memset(devices, sizeof(devices), 0);
	numDevices = 0;

	if(DetectPresence(B(OWBUS1)))
	_ds1820_search_busi(B(OWBUS1));
	if(DetectPresence(B(OWBUS2)))
	_ds1820_search_busi(B(OWBUS2));
	if(DetectPresence(B(OWBUS3)))
	_ds1820_search_busi(B(OWBUS3));
	//usb_flush_output();
}


char _ds1820_start_temp(char bus)
{
	if(!DetectPresence(bus))
	return -1;

	SendByte(ROM_SKIP, bus);
	SendByte(DS1820_START_CONVERSION, bus);
	return 0;
}


char ds_1820_start_temps()
{
	_ds1820_start_temp(B(OWBUS1));
	_ds1820_start_temp(B(OWBUS2));
	_ds1820_start_temp(B(OWBUS3));
	return 0;
}


char ds_1820_read_temps()
{
	for (int ii = 0; ii < numDevices; ii++)
	{
		unsigned char bus = devices[ii].bus;
		unsigned char *id = devices[ii].id;
		if(!DetectPresence(bus))
		continue;

		MatchRom(id, bus);
		SendByte(DS1820_READ_SCRATCHPAD, bus);
		signed int temperature = ReceiveByte(bus);
		temperature |= (ReceiveByte(bus) << 8);
		usb_printf_P(PSTR("T%02X%02X%02X%02X%02X%02X%02X%02X=%d\r\n"), id[7], id[6], id[5], id[4], id[3], id[2], id[1], id[0], temperature);
	}

	usb_flush_output();
	return 0;
}
