/**
 * This code was found somewhere on the Internet way back in the day, I can't find/remember where.
 * It was example code for using a Dallas one-wire bus on an AVR.  Maybe someone will let me know so I can
 * properly reference it.
 */

/****************************************************************************
 ROM commands
****************************************************************************/
#define ROM_READ    0x33    //!< READ ROM command code.
#define ROM_SKIP    0xcc    //!< SKIP ROM command code.
#define ROM_MATCH   0x55    //!< MATCH ROM command code.
#define ROM_SEARCH  0xf0    //!< SEARCH ROM command code.

/****************************************************************************
 Declarations
****************************************************************************/
unsigned char ComputeCRC8(unsigned char inData, unsigned char seed);
unsigned int ComputeCRC16(unsigned char inData, unsigned int seed);
unsigned char CheckRomCRC(unsigned char * romValue);
void OWInit(unsigned char pins);
void WriteBit1(unsigned char pins);
void WriteBit0(unsigned char pins);
unsigned char ReadBit(unsigned char pins);
unsigned char DetectPresence(unsigned char pins);
void SendByte(unsigned char data, unsigned char pins);
unsigned char ReceiveByte(unsigned char pin);
void SkipRom(unsigned char pins);
void ReadRom(unsigned char * romValue, unsigned char pins);
void MatchRom(unsigned char * romValue, unsigned char pins);
unsigned char SearchRom(unsigned char * bitPattern, unsigned char lastDeviation, unsigned char pins);
