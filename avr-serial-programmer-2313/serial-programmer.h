/*****************************************************************************
*           Serial Programmer for AVR
*       Ken Sarkies ksarkies@trinity.asn.au
*
* File              : serial-programmer.h
* Compiler          : AVR-GCC/avr-libc(>= 1.2.5)
* Revision          : $Revision: 0.1 $
* Updated by        : $ author K. Sarkies 14/11/2010 $
*
* Target platform   : AT90S2313 or ATTiny2313
*
****************************************************************************/

/* baud rate register value calculation */
#ifndef F_CPU
#define F_CPU    8000000
#endif
#define BAUD_RATE   19200

// FLASH Pagesize in words
#define FPAGESIZE   32
// EEPROM Pagesize in words
#define EPAGESIZE   4

/* define pin for entering self programming mode */
#define SCK         PB7		// SCK   pin of the target (output)
#define MISO        PB6		// MISO  pin of the target (input)
#define MOSI        PB5		// MOSI  pin of the target (output)
#define RESET       PB4		// RESET pin of the target (output)
#define PASSTHROUGH PB3     // Pull low to allow serial port passthrough to target
#define LEDPROG     PB1		// dual color LED output, anode green  (output)
#define LED         PB0		// LED output, active low, dual color LED cathode green (output)

unsigned char BlockLoad(const unsigned int size,
                        const unsigned char mem,
                        uint16_t *address);
void BlockRead(const unsigned int size,
                        const unsigned char mem,
                        uint16_t *address);
uint8_t writeByte(const uint8_t datum);
void writeCommand(uint8_t, uint8_t, uint8_t, uint8_t);
void pollDelay(const uint8_t shortDelay);

