/*  Atmel Serial Programmer for AVR 2313 version */

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

