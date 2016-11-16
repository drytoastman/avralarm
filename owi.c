/**
 * This code was found somewhere on the Internet way back in the day, I can't find/remember where.
 * It was example code for using a Dallas one-wire bus on an AVR.  Maybe someone will let me know so I can
 * properly reference it.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "owi.h"
#include "ports.h"

// 4 cycles for bus movement @ 4Mhz = 1uS
#define BUSINSOFF (4e6/F_CPU)
#define DELAY_A (6   - BUSINSOFF)
#define DELAY_B (64  - BUSINSOFF)
#define DELAY_C (60  - BUSINSOFF)
#define DELAY_D (10  - BUSINSOFF)
#define DELAY_E (9   - BUSINSOFF)
#define DELAY_F (55  - BUSINSOFF)
#define DELAY_G (0   - BUSINSOFF)
#define DELAY_H (480 - BUSINSOFF)
#define DELAY_I (70  - BUSINSOFF)
#define DELAY_J (410 - BUSINSOFF)

#define CRC_OK      0x00    //!< CRC check succeded
#define CRC_ERROR   0x01    //!< CRC check failed

/****************************************************************************
 Return codes
****************************************************************************/
#define ROM_SEARCH_FINISHED     0x00    //!< Search finished return code.
#define ROM_SEARCH_FAILED       0xff    //!< Search failed return code.

/****************************************************************************
 Macros
****************************************************************************/
#define PULL_BUS_LOW(bitMask) OWDDR |= bitMask; OWPORT &= ~bitMask;
#define RELEASE_BUS(bitMask) OWDDR &= ~bitMask; OWPORT |= bitMask;

unsigned char ComputeCRC8(unsigned char inData, unsigned char seed)
{
    unsigned char bitsLeft;
    unsigned char temp;

    for (bitsLeft = 8; bitsLeft > 0; bitsLeft--)
    {
        temp = ((seed ^ inData) & 0x01);
        if (temp == 0)
        {
            seed >>= 1;
        }
        else
        {
            seed ^= 0x18;
            seed >>= 1;
            seed |= 0x80;
        }
        inData >>= 1;
    }
    return seed;    
}

unsigned int ComputeCRC16(unsigned char inData, unsigned int seed)
{
    unsigned char bitsLeft;
    unsigned char temp;

    for (bitsLeft = 8; bitsLeft > 0; bitsLeft--)
    {
        temp = ((seed ^ inData) & 0x01);
        if (temp == 0)
        {
            seed >>= 1;
        }
        else
        {
            seed ^= 0x4002;
            seed >>= 1;
            seed |= 0x8000;
        }
        inData >>= 1;
    }
    return seed;    
}

unsigned char CheckRomCRC(unsigned char * romValue)
{
    unsigned char i;
    unsigned char crc8 = 0;
    
    for (i = 0; i < 7; i++)
    {
        crc8 = ComputeCRC8(*romValue, crc8);
        romValue++;
    }
    if (crc8 == (*romValue))
    {
        return CRC_OK;
    }
    return CRC_ERROR;
}




void WriteBit1(unsigned char pins)
{
    cli();
    PULL_BUS_LOW(pins);
    _delay_us(DELAY_A);
    RELEASE_BUS(pins);
    _delay_us(DELAY_B);
    sei();
}

void WriteBit0(unsigned char pins)
{
    cli();
    PULL_BUS_LOW(pins);
    _delay_us(DELAY_C);
    RELEASE_BUS(pins);
    _delay_us(DELAY_D);
    sei();
}

unsigned char ReadBit(unsigned char pins)
{
    unsigned char bitsRead;
    cli();
    PULL_BUS_LOW(pins);
    _delay_us(DELAY_A);
    RELEASE_BUS(pins);
    _delay_us(DELAY_E);
    bitsRead = OWPIN & pins;
    _delay_us(DELAY_F);
    sei();
    return bitsRead;
}

unsigned char DetectPresence(unsigned char pins)
{
    unsigned char presenceDetected;
    cli();
    PULL_BUS_LOW(pins);
    _delay_us(DELAY_H);
    RELEASE_BUS(pins);
    _delay_us(DELAY_I);
    presenceDetected = ((~OWPIN) & pins);
    _delay_us(DELAY_J);
    sei();
    return presenceDetected;
}


void SendByte(unsigned char data, unsigned char pins)
{
    unsigned char temp;
    unsigned char i;
    
    // Do once for each bit
    for (i = 0; i < 8; i++)
    {
        // Determine if lsb is '0' or '1' and transmit corresponding
        // waveform on the bus.
        temp = data & 0x01;
        if (temp)
        {
            WriteBit1(pins);
        }
        else
        {
            WriteBit0(pins);
        }
        // Right shift the data to get next bit.
        data >>= 1;
    }
}


unsigned char ReceiveByte(unsigned char pin)
{
    unsigned char data;
    unsigned char i;

    // Clear the temporary input variable.
    data = 0x00;
    
    // Do once for each bit
    for (i = 0; i < 8; i++)
    {
        // Shift temporary input variable right.
        data >>= 1;
        // Set the msb if a '1' value is read from the bus.
        // Leave as it is ('0') else.
        if (ReadBit(pin))
        {
            // Set msb
            data |= 0x80;
        }
    }
    return data;
}


void SkipRom(unsigned char pins)
{
    // Send the SKIP ROM command on the bus.
    SendByte(ROM_SKIP, pins);
}


void ReadRom(unsigned char * romValue, unsigned char pin)
{
    unsigned char bytesLeft = 8;

    // Send the READ ROM command on the bus.
    SendByte(ROM_READ, pin);
    
    // Do 8 times.
    while (bytesLeft > 0)
    {
        // Place the received data in memory.
        *romValue++ = ReceiveByte(pin);
        bytesLeft--;
    }
}


void MatchRom(unsigned char * romValue, unsigned char pins)
{
    unsigned char bytesLeft = 8;   
    
    // Send the MATCH ROM command.
    SendByte(ROM_MATCH, pins);

    // Do once for each byte.
    while (bytesLeft > 0)
    {
        // Transmit 1 byte of the ID to match.
        SendByte(*romValue++, pins);
        bytesLeft--;
    }
}


/*! \brief  Sends the SEARCH ROM command and returns 1 id found on the 
 *          1-Wire(R) bus.
 *
 *  \param  bitPattern      A pointer to an 8 byte char array where the 
 *                          discovered identifier will be placed. When 
 *                          searching for several slaves, a copy of the 
 *                          last found identifier should be supplied in 
 *                          the array, or the search will fail.
 *
 *  \param  lastDeviation   The bit position where the algorithm made a 
 *                          choice the last time it was run. This argument 
 *                          should be 0 when a search is initiated. Supplying 
 *                          the return argument of this function when calling 
 *                          repeatedly will go through the complete slave 
 *                          search.
 *
 *  \param  pin             A bit-mask of the bus to perform a ROM search on.
 *
 *  \return The last bit position where there was a discrepancy between slave addresses the last time this function was run. Returns ROM_SEARCH_FAILED if an error was detected (e.g. a device was connected to the bus during the search), or ROM_SEARCH_FINISHED when there are no more devices to be discovered.
 *
 *  \note   See main.c for an example of how to utilize this function.
 */
unsigned char SearchRom(unsigned char * bitPattern, unsigned char lastDeviation, unsigned char pin)
{
    unsigned char currentBit = 1;
    unsigned char newDeviation = 0;
    unsigned char bitMask = 0x01;
    unsigned char bitA;
    unsigned char bitB;

    // Send SEARCH ROM command on the bus.
    SendByte(ROM_SEARCH, pin);
    
    // Walk through all 64 bits.
    while (currentBit <= 64)
    {
        // Read bit from bus twice.
        bitA = ReadBit(pin);
        bitB = ReadBit(pin);

        if (bitA && bitB)
        {
            // Both bits 1 (Error).
			//usb_printf_P(PSTR("SearchFailed at %d\r\n"), currentBit);
            newDeviation = 0;
            return ROM_SEARCH_FAILED;
        }
        else if (bitA ^ bitB)
        {
            // Bits A and B are different. All devices have the same bit here.
            // Set the bit in bitPattern to this value.
            if (bitA)
            {
                (*bitPattern) |= bitMask;
            }
            else
            {
                (*bitPattern) &= ~bitMask;
            }
        }
        else // Both bits 0
        {
            // If this is where a choice was made the last time,
            // a '1' bit is selected this time.
            if (currentBit == lastDeviation)
            {
                (*bitPattern) |= bitMask;
            }
            // For the rest of the id, '0' bits are selected when
            // discrepancies occur.
            else if (currentBit > lastDeviation)
            {
                (*bitPattern) &= ~bitMask;
                newDeviation = currentBit;
            }
            // If current bit in bit pattern = 0, then this is
            // out new deviation.
            else if ( !(*bitPattern & bitMask)) 
            {
                newDeviation = currentBit;
            }
            // IF the bit is already 1, do nothing.
            else
            {
            
            }
        }
                
        
        // Send the selected bit to the bus.
        if ((*bitPattern) & bitMask)
        {
            WriteBit1(pin);
        }
        else
        {
            WriteBit0(pin);
        }

        // Increment current bit.    
        currentBit++;

        // Adjust bitMask and bitPattern pointer.    
        bitMask <<= 1;
        if (!bitMask)
        {
            bitMask = 0x01;
            bitPattern++;
        }
    }
    return newDeviation;
}



