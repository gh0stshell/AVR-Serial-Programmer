/**
@mainpage Atmel Serial Programmer for AVR 4313 version
@version 0.2
@author Ken Sarkies (www.jiggerjuice.net)
@date 15 January 2012

@brief Code for the embedded portion of the Battery Charger Project,
for an 8MHz Atmel MCU of the family AT90S2313.

@details This is the AVR code for the stand-alone serial programmer that
interfaces to the PC via a serial (USB) link and to the target via the SPI
interface.

The board is designed to receive serial commands that are compatible with the
AVRPROG command set, and to activate the SPI interface to program a target AVR
microcontroller. The board uses the AT90S2313 (probably obsolete) AVR
microcontroller (which can be replaced by the ATTiny2313 and ATTiny4313).

The SPI port is used along with an additional output pin used to hold the target
in a reset state. When programming has been completed, the board switches the
serial communications to pass through to the target. This switches the incoming
serial communications from the target through to the PC. The outgoing
communications from the PC are simply copied to the target.

Refer to the Project documents for more details.

The main differences between targets are:
1. The availability of a busy status. If not available, fixed delays need to be
   added where memory accesses are made, or rereading of the memory location.
2. The use of paged or individual FLASH memory programming. EEPROM can always be
   accessed individually, but some devices allow paged writes.
3. The size of the memory pages. The AT90S2313 and ATTiny2313 are limited to 32
   word page support.
4. The need to access Fuse, High Fuse and Extended Fuse bits.
5. While all have programmable lock bits, not all can be read.

Currently all the above are supported and no distinction is made.

@note
Compiler: avr-gcc (GCC) 3.4.5 (CDK4AVR 3.0 3.4.5-20060708)
@note
Target: AT90S2313, ATTiny2313, ATTiny4313 at 8.0MHz
@note
Tested: AT90S2313, ATTiny2313, ATTiny4313 at 8.0MHz
@note
Uses: serial.c, defines.h, flash.h written by R Aapeland (?)
@todo Where a fuse/lock capability is tested and the command within the if
statement contains a recchar() call, move the latter out of the if to allow the
protocol to complete correctly even if the capability is not supported.
*/
/****************************************************************************
 *   Copyright (C) 2007 by Ken Sarkies ksarkies@internode.on.net            *
 *                                                                          *
 *   This file is part of serial-programmer                                 *
 *                                                                          *
 *   serial-programmer is free software; you can redistribute it and/or     *
 *   modify it under the terms of the GNU General Public License as         *
 *   published by the Free Software Foundation; either version 2 of the     *
 *   License, or (at your option) any later version.                        *
 *                                                                          *
 *   serial-programmer is distributed in the hope that it will be useful,   *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *   GNU General Public License for more details.                           *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with serial-programmer. If not, write to the Free Software       *
 *   Foundation, Inc.,                                                      *
 *   51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA.              *
 ***************************************************************************/

#include <inttypes.h>
#include <avr/eeprom.h>
#include <avr/sfr_defs.h>
#include <avr/io.h>
#include <avr/wdt.h>

// Definitions of microcontroller registers and other characteristics
#define	_ATtiny2313
//#define	_AT90S2313
#ifdef	__ICCAVR__
#include "iotn2313.h"
#elif	__GNUC__
#include <avr/io.h>
#endif

#define TRUE 1
#define FALSE 0
/* Convenience macros (we don't use them all) */
#define  _BV(bit) (1 << (bit))
#define  inb(sfr) _SFR_BYTE(sfr)
#define  inw(sfr) _SFR_WORD(sfr)
#define  outb(sfr, val) (_SFR_BYTE(sfr) = (val))
#define  outw(sfr, val) (_SFR_WORD(sfr) = (val))
#define  cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define  sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#define  high(x) ((uint8_t) (x >> 8) & 0xFF)
#define  low(x) ((uint8_t) (x & 0xFF))

#include "serial-programmer.h"
#include <util/delay.h>

/*****************************************************************************/
/* Functions to send/receive */

/* This defines our baudrate 25=19200, 12=38400, 8=57600 */
#define BRREG_VALUE             12

/* definitions for UART control */
#ifdef _ATtiny2313
#define	UART_STATUS UCSRA
#endif
#ifdef _AT90S2313
#define	UART_STATUS USR
#endif

/*****************************************************************************/
void initbootuart(void)
{
#ifdef _ATtiny2313
    UBRRL = BRREG_VALUE;
    UBRRH = 0;
    UCSRA = 0;
    UCSRB = (1 << RXEN) | (1 << TXEN);  // enable receive and transmit 
    UCSRC = 6;                          // Set to 8-bit mode
#endif
#ifdef _AT90S2313
    UBRR = BRREG_VALUE;
    USR = (1 << RXEN) | (1 << TXEN);  // enable receive and transmit 
#endif
}

/*****************************************************************************/
void sendchar(unsigned char c)
{
    UDR = c;                                // Load data to Tx buffer
    while (!(UART_STATUS & (1 << TXC)));    // wait until sent
    UART_STATUS |= (1 << TXC);              // clear TXCflag
}

/*****************************************************************************/
unsigned char recchar(void)
{
  while(!(UART_STATUS & (1 << RXC)));       // wait for data
  return UDR;
}

/*****************************************************************************/
/** @brief Array of parts and properties

This lists all the target devices supported by the programmer.

The first signature byte is assumed to be always E1. If not, then we abort
programming.
Flash and EEPROM page sizes zero means that page writes are not supported.
Lock and Fuse support is given by a bitwise quantity.
0 = Lock Read
1 = Fuse Read
2 = High Fuse Read
3 = Extended Fuse Read
4 = Lock Write
5 = Fuse Write
6 = High Fuse Write
7 = Extended Fuse Write
*/

/* Define the parts to be stored into EEPROM.
We don't do the AT90S2313 yet as it requires special code development
FPage is the FLASH  pagesize in words
EPage is the EEPROM pagesize in bytes
Busy indicates if the programming hardware provides a busy flag
*/
#define NUMPARTS 18
uint8_t part[NUMPARTS][6] = {
/* Sig 2, Sig 3, FPage, EPage, Busy, Lock/Fuse */
//{   0x91,  0x01,    0,    0,   FALSE,  0x10     },  // AT90S2313
{   0x91,  0x0B,   16,    4,   TRUE,   0xFF     },  // ATTiny24
{   0x91,  0x09,   16,    0,   FALSE,  0x77     },  // ATTiny26
{   0x91,  0x0A,   16,    4,   TRUE,   0xFF     },  // ATTiny2313
{   0x91,  0x0C,   16,    4,   TRUE,   0xFF     },  // ATTiny261
{   0x92,  0x07,   32,    4,   TRUE,   0xFF     },  // ATTiny44
{   0x92,  0x0D,   32,    4,   TRUE,   0xFF     },  // ATTiny4313
{   0x92,  0x05,   32,    4,   TRUE,   0xFF     },  // ATMega48
{   0x92,  0x08,   32,    4,   TRUE,   0xFF     },  // ATTiny461
{   0x92,  0x15,    8,    4,   TRUE,   0xFF     },  // ATTiny441
{   0x93,  0x0C,   32,    4,   TRUE,   0xFF     },  // ATTiny84
{   0x93,  0x08,   32,    0,   FALSE,  0x77     },  // ATMega8535
{   0x93,  0x0A,   32,    4,   TRUE,   0xFF     },  // ATMega88
{   0x93,  0x0D,   32,    4,   TRUE,   0xFF     },  // ATTiny861
{   0x93,  0x15,    8,    4,   TRUE,   0xFF     },  // ATTiny841
{   0x94,  0x03,   64,    4,   TRUE,   0x77     },  // ATMega16
{   0x94,  0x06,   64,    4,   TRUE,   0xFF     },  // ATMega168
{   0x95,  0x0F,   64,    4,   TRUE,   0xFF     },  // ATMega328
{   0x95,  0x02,   64,    0,   FALSE,  0x77     }   // ATMega32
};
/*****************************************************************************/

uint16_t address;                   // Address to program
uint8_t command;                    // received instruction character
uint16_t tempInt;
uint8_t lsbAddress;
uint8_t msbAddress;
uint8_t buffer[4];                  // Buffer to store response from target
uint8_t fPageSize;
uint8_t ePageSize;
uint8_t canCheckBusy;
uint8_t lfCapability;
uint8_t received;

int main(void)
{

/*  Initialise hardware timers and ports. All start off as inputs */
/*  PD0 is RxD
    PD1 is TxD */
/** PORTB is set as outputs on bits 0,3,4 to set controls.
    The SPI output ports SCK and MOSI stay as inputs until ready to program.
    Set the Ports PB0-2 as outputs and set high to turn off all LEDs
    PB0 = programming mode LED
    PB3 = programming completed LED (and serial comms switch over)
    PB4 = programming active LED (and reset out)
    PB5 = MOSI
    PB6 = MISO
    PB7 = SCK */

    sbi(ACSR,7);                        // Turn off Analogue Comparator
    initbootuart();           	        // Initialize UART.
    uint8_t sigByte1=0;                 // Target Definition defaults
    uint8_t sigByte2=0;
    uint8_t sigByte3=0;
    uint8_t fuseBits=0;
    uint8_t highFuseBits=0;
    uint8_t extendedFuseBits=0;
    uint8_t lockBits=0;

/*---------------------------------------------------------------------------*/
/* Main loop. We exit this only with an "E" command. */
    {
        for(;;)
        {
            command=recchar();          // Loop and wait for command character.

/** 'a' Check autoincrement status.
This allows a block of data to be uploaded to consecutive addresses without
specifying the address each time */
            if (command=='a')
            {
                sendchar('Y');          // Yes, we will autoincrement.
            }

/** 'A' Set address.
This is stored and incremented for each upload/download.
NOTE: Flash addresses are given as word addresses, not byte addresses. */
            else if (command=='A')      // Set address
            {
                address = (recchar()<<8);   // Set address high byte first.
                address |= recchar();       // Then low byte.
                sendchar('\r');             // Send OK back.
            }

/** 'b' Check block load support. This returns the allowed block size.
We will not buffer anything so we will use the FLASH page size to limit
the programmer's blocks to those that will fit the target's page. This then
avoids the long delay when the page is committed, that may cause incoming data
to be lost. This should not be called before the P command is executed, and the
target device characteristics obtained. The fPageSize characteristic should be
non zero.*/
           else if (command=='b')
            {
                uint16_t blockLength = ((uint16_t)fPageSize<<1);
                if (fPageSize > 0) sendchar('Y');   // Report block load supported.
                else sendchar('N');
                sendchar(high(blockLength));        // MSB first.
                sendchar(low(blockLength));         // Report FLASH pagesize (bytes).
            }

/** 'p' Get programmer type. This returns just 'S' for serial. */
            else if (command=='p')
            {
                sendchar('S');                      // Answer 'SERIAL'.
            }

/** 'S' Return programmer identifier. Always 7 digits. We call it AVRSPRG */
            else if (command=='S')
            {
                sendchar('A');
                sendchar('V');                      // ID always 7 characters.
                sendchar('R');
                sendchar('S');
                sendchar('P');
                sendchar('R');
                sendchar('G');
            }

/** 'V' Return software version. */
            else if (command=='V')
            {
                sendchar('0');
                sendchar('0');
            }

/** 't' Return supported device codes. This returns a list of devices that can
be programmed. This is only used by AVRPROG so we will not use it - we work with
signature bytes instead. */
            else if(command=='t')
            {
                sendchar( 0 );                          // Send list terminator.
            }

/** 'x' Set LED. */
            else if ((command=='x') || (command=='y') || (command=='T'))
            {
//                if (command=='x') sbi(PORTB,LED);
//                else if (command=='y') cbi(PORTB,LED);
                recchar();                              // Discard sent value
                sendchar('\r');                         // Send OK back.
            }

/** 'P' Enter programming mode.
This starts the programming of the device. Pulse the reset line high while SCK
is low. Send the command and ensure that the echoed second byte is correct,
otherwise redo. With this we get the device signature and search the table for
its characteristics. A timeout is provided in case the device doesn't respond.
This will allow fall through to an ultimate error response.
The reset line is held low until programming mode is exited. */
            else if (command=='P')
            {
                outb(DDRB,(inb(DDRB) | 0xB9));      // Setup SPI output ports
                outb(PORTB,(inb(PORTB) | 0xB9));    // SCK and MOSI high, and LEDs off
                uint8_t retry = 10;
                uint8_t result = 0;
                while ((result != 0x53) && (retry-- > 0))
                {
                    cbi(PORTB,SCK);                 // Set serial clock low
                    sbi(PORTB,RESET);               // Pulse reset line off
// Delay to let CPU know that programming will occur
                    _delay_us(100);
                    cbi(PORTB,RESET);               // Pulse reset line on
                    _delay_us(25000);               // 25ms delay
                    writeCommand(0xAC,0x53,0x00,0x00);  // "Start programming" command
                    result=buffer[2];
                }
/** Once we are in programming mode, grab the signature bytes and extract all information
about the target device such as its memory sizes, page sizes and capabilities. */
                writeCommand(0x30,0x00,0x00,0x00);
                sigByte1 = buffer[3];
                writeCommand(0x30,0x00,0x01,0x00);
                sigByte2 = buffer[3];
                writeCommand(0x30,0x00,0x02,0x00);  // Signature Bytes
                sigByte3 = buffer[3];
/* Check for device support. If the first signature byte is not 1E, then the device is
either not an Atmel device, is locked, or is not responding.*/
// Indicate if the target device is supported.
                uint8_t found=FALSE;
                uint8_t partNo = 0;
                if (sigByte1 == 0x1E)
                {
                    while ((partNo < NUMPARTS) && (! found))
                    {
                        found = ((part[partNo][0] == sigByte2) &&
                                (part[partNo][1] == sigByte3));
                        partNo++;
                    }
                }
                if (found)
                {
                    partNo--;
                    sendchar('\r');
                    fPageSize = part[partNo][2];
                    ePageSize = part[partNo][3];
                    canCheckBusy = part[partNo][4];
                    lfCapability = part[partNo][5];
                    buffer[3] = 0;                      // In case we cannot read these
                    if (lfCapability & 0x08)
                        writeCommand(0x50,0x08,0x00,0x00);  // Read Extended Fuse Bits
                    extendedFuseBits = buffer[3];
                    if (lfCapability & 0x04)
                        writeCommand(0x58,0x08,0x00,0x00);  // Read High Fuse Bits
                    highFuseBits = buffer[3];
                    if (lfCapability & 0x02)
                        writeCommand(0x50,0x00,0x00,0x00);  // Read Fuse Bits
                    fuseBits = buffer[3];
                    if (lfCapability & 0x01)
                        writeCommand(0x58,0x00,0x00,0x00);  // Read Lock Bits
                    lockBits = buffer[3];
                }
                else                                    // Not found?
                {
                    sbi(PORTB,RESET);                   // Lift reset line
                    sendchar('?');                      // Device cannot be programmed
                    outb(DDRB,(inb(DDRB) & ~0xA0));     // Set SPI ports to inputs
                }
            }

/** 'L' Leave programming mode. */
            else if(command=='L')
            {
                sbi(PORTB,RESET);                       // Turn reset line off
                sendchar('\r');                         // Answer OK.
                outb(DDRB,(inb(DDRB) & ~0xA0));         // Set SPI ports to inputs
            }

/** 'e' Chip erase.
This requires several ms. Ensure the command has finished before acknowledging. */
            else if (command=='e')
            {
                writeCommand(0xAC,0x80,0x00,0x00);      // Erase command
                pollDelay(FALSE);
                sendchar('\r');                         // Send OK back.
            }

/** 'R' Read program memory */
            else if (command=='R')
            {
/** Send high byte, then low byte of flash word.
Send each byte from the address specified (note address variable is modified).*/
                lsbAddress = low(address);
                msbAddress = high(address);
                writeCommand(0x28,msbAddress,lsbAddress,0x00);  // Read high byte
                sendchar(buffer[3]);
                writeCommand(0x20,msbAddress,lsbAddress,0x00);  // Read low byte
                sendchar(buffer[3]);
                address++;                              // Auto-advance to next Flash word.
            }

/** 'c' Write to program memory page buffer, low byte.
NOTE: Always use this command before sending the high byte. It is written to the
page but the address is not incremented.*/
            else if (command=='c')
            {
                received = recchar();
                writeCommand(0x40,0x00,address & 0x7F,received);    // Low byte
                sendchar('\r');                         // Send OK back.
            }

/** 'C' Write to program memory page buffer, high byte.
This will cause the word to be written to the page and the address incremented. It is
the responsibility of the user to issue a page write command.*/
            else if (command=='C')
            {
                received = recchar();
                writeCommand(0x48,0x00,address & 0x7F,received);    // High Byte
                address++;                              // Auto-advance to next Flash word.
                sendchar('\r');                         // Send OK back.
            }

/** 'm' Issue Page Write. This writes the target device page buffer to the
Flash. The address is that of the page, with the lower bits masked out. This
requires several ms. Ensure that the command has finished before acknowledging.
We could check for end of memory here but that would require storing the Flash
capacity for each device. The calling program will know in any case if it has
overstepped.*/
            else if (command== 'm')
            {
// Write Page
                writeCommand(0x4C,(address>>8) & 0x7F,address & 0xE0,0x00);
                pollDelay(TRUE);                        // Short delay
                sendchar('\r');                         // Send OK back.
            }
/** 'D' Write EEPROM memory
This writes the byte directly to the EEPROM at the specified address.
This requires several ms. Ensure that the command has finished before acknowledging.*/
            else if (command == 'D')
            {
                lsbAddress = low(address);
                msbAddress = high(address);
// EEPROM byte
                writeCommand(0xC0,msbAddress,lsbAddress,recchar());
                address++;                              // Auto-advance to next EEPROM byte.
                pollDelay(FALSE);                       // Long delay
                sendchar('\r');                         // Send OK back.
            }

/** 'd' Read EEPROM memory */
            else if (command == 'd')
            {
                lsbAddress = low(address);
                msbAddress = high(address);
                writeCommand(0xA0,msbAddress,lsbAddress, 0x00);
                sendchar(buffer[3]);
                address++;                              // Auto-advance to next EEPROM byte.
            }

/** 'B' Start block load.
 The address must have already been set otherwise it will be undefined. */
            else if (command=='B')
            {
                tempInt = (recchar()<<8);               // Get block size high byte first.
                tempInt |= recchar();                   // Low Byte.
                sendchar(BlockLoad(tempInt,recchar(),&address));  // Block load.
            }

/** 'g' Start block read.
 The address must have already been set otherwise it will be undefined.*/
            else if (command=='g')
            {
                tempInt = (recchar()<<8);               // Get block size high byte first.
                tempInt |= recchar();                   // Low Byte.
                command = recchar();                    // Get memory type
                BlockRead(tempInt,command,&address);    // Block read
            }
/** 'r' Read lock bits. */
            else if (command=='r')
            {
                sendchar(lockBits);
            }

/** 'l' Write lockbits. */
            else if (command=='l')
            {
                if (lfCapability & 0x10) writeCommand(0xAC,0xE0,0x00,recchar());
                sendchar('\r');                         // Send OK back.
            }

/** 'F' Read fuse bits. */
            else if (command=='F')
            {
                sendchar(fuseBits);
            }

/** 'f' Write fuse bits. */
            else if (command=='f')
            {
// Fuse byte
                if (lfCapability & 0x20) writeCommand(0xAC,0xA0,0x00,recchar());
                sendchar('\r');                         // Send OK back.
            }

/** 'N' Read high fuse bits. */
            else if (command=='N')
            {
                sendchar(highFuseBits);
            }

/** 'n' Write high fuse bits. */
            else if (command=='n')
            {
// High Fuse byte
                if (lfCapability & 0x40) writeCommand(0xAC,0xA8,0x00,recchar());
                sendchar('\r');                         // Send OK back.
            }

/** 'Q' Read extended fuse bits. */
            else if (command=='Q')
            {
                sendchar(extendedFuseBits);
            }

/** 'q' Write extended fuse bits. */
            else if (command=='q')
            {
// Extended Fuse byte
                if (lfCapability & 0x80) writeCommand(0xAC,0xA4,0x00,recchar());
                sendchar('\r');                         // Send OK back.
            }

/** 's' Return signature bytes. Sent Most Significant Byte first. */
            else if (command=='s')
            {
                sendchar(sigByte3);
                sendchar(sigByte2);
                sendchar(sigByte1);
            }

/** 'E' Exit bootloader.
At this command we enter serial passthrough and don't return from it until a
hardware reset occurs.
At the same time we should lift the reset from the target. Spin endlessly so we
don't interpret serial data, and wait for our own hard reset.*/
            else if (command=='E')
            {
                sendchar('\r');
                sbi(PORTB,RESET);               // Pulse reset line off
                cbi(PORTB,PASSTHROUGH);         // Change to serial passthrough
                outb(DDRB,(inb(DDRB) & ~0xA0)); // Set SPI ports to inputs
                for (;;);                    // Spin endlessly
            }

/** The last command to accept is ESC (synchronization).
This is used to abort any command by filling in remaining parameters, after
which it is simply ignored.
Otherwise, the command is not recognized and a "?" is returned.*/
            else if (command!=0x1b) sendchar('?');  // If not ESC, then it is unrecognized
        }
    }
}

/********************************************************************************/
/** @brief Write a block to application memory

Block Load can be used to reduce the traffic on the serial link.

The block is read in and is written page by page to the AVR internal page buffer,
each page followed by a page write. The program allows for blocksizes larger
than the target page buffer. The EEPROM internal buffer is typically only 4
bytes, while the FLASH buffer can be from 16 to 64 words.

Take care that EEPROM addresses are given in bytes, while FLASH addresses are
in words.

The code is a bit involved when dealing with non-paged support. We do not use
pages at all so pageSize is irrelevant and ends up being -1, which in unsigned
arithmetic should be all 1's. However it is used to set the addresses in both
paged and non- paged modes, so we may see some side-effects here. It should be
worked a bit better but space is running out. In the devices, pages are always
used when the memory is above a certain size, so 8-bit addressing should be OK.

@param[in] size: Size of the transfer in bytes
@param[in] mem:  Memory type ('E' or 'F')
@param[in] *address: pointer to the memory address in bytes (EEPROM) or words (FLASH)
@returns response ('?' or '\\r') to return to the PC
*/

unsigned char BlockLoad(uint16_t size, unsigned char mem, uint16_t *address)
{
    uint16_t blockCount = 0;
    uint16_t pageOffset = 0;
    uint16_t pageMask;
    if (mem=='E')
        pageMask = ((uint16_t)ePageSize-1);
    else if (mem=='F')
        pageMask = ((uint16_t)fPageSize-1);
    else return '?';                                        // Invalid Type
    uint16_t pageAddress = (*address) & (~pageMask);        // Upper bits identify page
    do
    {
        uint8_t lsbAddress = (*address) & pageMask;         // Address within page
        if (mem=='E')
        {
            if (ePageSize == 0)                             // Check Page Capability
            {
// Get and write EEPROM byte direct
                writeCommand(0xC0,0x00,lsbAddress,recchar());
                pollDelay(FALSE);   // Long wait for completion of write command
            }
            else
// Get and write EEPROM byte to its page
                writeCommand(0xC1,0x00,lsbAddress,recchar());
            blockCount+=1;
        }
        else
        {
            writeCommand(0x40,0x00,lsbAddress,recchar());   // FLASH Low byte
            writeCommand(0x48,0x00,lsbAddress,recchar());   // FLASH High Byte
// Short wait for completion of write command
            if (fPageSize == 0) pollDelay(TRUE);
            blockCount+=2;
        }
        (*address)++;                                       // Select next byte/word location.
// Commit page. If paged writes are not used, skip this section. All writing is completed above.
        if (!(((mem=='E') && (ePageSize == 0)) || ((mem=='F') && (fPageSize == 0))))
        {
            pageOffset++;                                   // Check if a page is ready to commit
            if ((pageOffset > pageMask) || (blockCount >= size))
            {
                if (mem=='E')
                {
// Commit EEPROM Page
                    writeCommand(0xC2,high(pageAddress),low(pageAddress),0x00);
// Long wait for completion of commit command
                    pollDelay(FALSE);
                }
                else
                {
// Commit FLASH Page
                    writeCommand(0x4C,high(pageAddress),low(pageAddress),0x00);
// Short wait for completion of commit command
                    pollDelay(TRUE);
                }
                pageAddress = (*address) & (~pageMask);     // next page
                pageOffset = 0;                             // Restore counter for next page
            }
        }
    }
    while (blockCount < size);
    return '\r';
}

/*****************************************************************************/
/** @brief Read a block from application memory

There is no block read as such in the SPI interface, just do it byte by byte.
This code allows the interface to reduce traffic by reading in a single
transaction.

Take care that EEPROM addresses are given in bytes, while FLASH addresses are
in words.

Note that the low byte is returned first, followed by the high byte. This
differs from the 'R' command in which the high byte is returned first.

@param[in] size: Size of the buffer in bytes
@param[in] mem:  Memory type ('E' or 'F')
@param[in] *address: pointer to memory address in bytes (EEPROM) or words (FLASH)
*/

void BlockRead(unsigned int size, unsigned char mem, uint16_t *address)
{
    {
        for(uint16_t n=0; n < size; n+=2)
        {
            lsbAddress = low(*address);
            msbAddress = high(*address);
            if (mem=='E')
                writeCommand(0xA0,msbAddress,lsbAddress,0x00);  // EEPROM Byte
            else
            {
                writeCommand(0x20,msbAddress,lsbAddress,0x00);  // FLASH Low Byte
                sendchar(buffer[3]);
                writeCommand(0x28,msbAddress,lsbAddress,0x00);  // FLASH High Byte
            }
            sendchar(buffer[3]);
            (*address)++;                                       // Select next FLASH word
        }
    }
}

/*****************************************************************************/
/** @brief Write a Byte to the SPI

Issue a write of a single byte on the SPI interface
Note that the data is transmitted and received MSB first.
Also note that the device is acting as a Master for the SPI bus,so MOSI is
output and MISO is input.

@param[in] datum: the byte to be written
@returns response: a byte read at the same time as the byte was written.
*/

uint8_t writeByte(const uint8_t datum)
{
    uint8_t value = datum;
    uint8_t response = 0;
    for (uint8_t n=0; n < 8;  n++)
    {
        outb(PORTB,(inb(PORTB) & ~_BV(MOSI))|((value & 0x80)>>(7-MOSI)));// Shift data MSB and put to MISO pin
        _delay_us(SPI_DELAY);       // Give him some time to settle
        sbi(PORTB,SCK);             // Raise SCK to latch output data
        _delay_us(SPI_DELAY);       // Give him some time to settle
        response <<= 1;             // Prepare response for next input bit
        response |= (inb(PINB) & _BV(MISO))>>MISO;    // Add in next bit read
        cbi(PORTB,SCK);             // Drop SCK ready for next time
        _delay_us(SPI_DELAY);       // Give him some time to settle
        value <<= 1;                // Move to expose next bit
    }
    return response;
}

/*****************************************************************************/
/** @brief Write a four byte Programming command to the SPI

Issue a write command on the SPI interface
This returns responses which (usually) match the command, one byte delayed.
The third byte read from the "start programming" command can be used to verify
sync. Other read commands return the read value in the fourth byte.

@param[in] cmd    SPI Programming command
@param[in] parm1  SPI Programming parameter 1
@param[in] parm2  SPI Programming parameter 2
@param[in] parm3  SPI Programming parameter 3
@returns The global buffer four bytes with the data returned from the command
*/

void writeCommand(const uint8_t cmd, const uint8_t parm1, const uint8_t parm2,
                  const uint8_t parm3)
{
    buffer[0] = writeByte(cmd);
    buffer[1] = writeByte(parm1);
    buffer[2] = writeByte(parm2);
    buffer[3] = writeByte(parm3);
}
/*****************************************************************************/
/** @brief Delay loop for writes.

The target capability for polling the busy status is used to determine if this
is to be used or if a fixed delay is needed. The delay is passed in the
parameter.

The compiler has a problem with passing a parameter to _delay_ms, and memory
usage bloats. So we need some fixed delays. Most likely _delay_us is too general
and includes floats.

Note the datasheet labels the flag as RDY, when in fact it states later that it
is the inverse, BSY, that is, wait until the flag drops to 0 before proceeding.

@param[in] shortDelay  Is the delay short?: TRUE for FLASH (4.5ms) or FALSE for
erase, EEPROM (9ms).
*/

void pollDelay(const uint8_t shortDelay)
{
    if (canCheckBusy)
    {
        do writeCommand(0xF0,0x00,0x00,0x00);       // This needs several ms, so
        while (buffer[3] & 0x01);                   // Wait for busy flag to drop
    }
    else
    {
        if (shortDelay) _delay_us(4500);
        else _delay_us(9000);
    }
}

