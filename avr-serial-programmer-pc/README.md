AVR Serial Programmer GUI
=========================

Tested:    ATMega88, ATMega48, ATMega168, ATMega16, ATMega32, ATMEga8535,
ATTiny2313, ATTiny461, ATTiny24, ATTiny44, ATTiny84, ATTiny4313.

Depends QT4 v4.7.4, qextserialport v1.2beta1

The Serial Programmer GUI is intended for use with an AVRPROG compatible Serial
Programmer or with AVR microcontrollers that have an AVR109 compatible
bootloader installed. Normally a bootloader would need to be obtained, compiled
and programmed into the microcontroller using a suitable programming tool. Once
this has been done, the microcontroller can be contacted over an RS232 serial
line to program an application into the device. The programmer begins by opening
the PC serial port ttyUSB0 for use with a Prolific compatible USB-serial
converter. This can be changed as necessary (for example to ttyS0 if you are
using a serial port) by editing the avrserialprogmain.cpp file.

The programmer synchronizes with the device using no parity, 1 stop bit and 8
bit data. A character 0xDD is sent and the character received back from the
device is checked for either a 0xDD or a "?". The latter indicates that the
bootloader may have been found. The 0xDD character relates to the packet
protocol used in the Acquisition project. If this is received, then a packet is
sent to instruct the device to jump into the bootloader. This can be removed if
the application protocol is not needed, but that is probably not necessary.
Needless to say if the bootloader is not present the device will probably just
reset itself. If neither character is received, the baud rate is cyclically
changed through a set of standard rates for a couple of cycles before giving up.

When the program has synchronized with the device, it presents an information
window. An Intel hex file can be opened and it will be immediately read and
loaded to the application area of the FLASH memory (a bootloader is never
overwritten by this; it can only be changed through the parallel or SPI
programming method).

The default baud rate to start with is 38400 baud. If this does not match that
of the device, then the program will cycle through standard baud rates between
2400 and 57600 baud until it finds the correct one. This process can be sped up
by changing the default baud rate to match that of the programmer or bootloader.
Care must be taken with choice of baud rate as the higher rates are not
necessarily produced accurately by an MCU. In addition there are time delays,
built into the code to adapt the program to the communication response time,
that may prevent the application from working reliably. The GUI was tested
successfully with 38400 baud and 8MHz MCUs. Repetitive programming failures,
particularly in block mode programming and verification, could be due to use
with an MCU of lower frequency that takes longer to process a programming
command.

The program allows for turning off block mode, and also allows verification
without programming and vice-versa. Autoaddress currently does nothing so it is
disabled. A debug switch allows masses of information to be dumped to standard
input. Dialogue boxes for programming lock and fuse bits are provided for a
small range of devices.

The application is presented as a QT dialogue box rather than a main window. It
is done this way to allow direct integration into other applications without
recoding. When the application is closed with the Close button it automatically
instructs the bootloader to jump into the program that was just loaded.

For the AVR microcontrollers, control of program loading to the FLASH memory,
and manipulating of the fuse and lock bits for configuring certain features of
the microcontroller, can be simply done through a PC parallel port programmer
(see the cdk4avr tools howto and references). Many of the AVRs provide a small
section of FLASH memory with capabilities that allow it to modify its own FLASH
memory. This is sometimes referred to as the bootblock. It normally resides in
the top of FLASH memory and can be set, through fuse bits, to different sizes,
typically 256 bytes to 4K bytes. The microcontroller can also be configured
through a fuse bit so that after a reset, a jump directly to the bootloader
section can be forced, rather than the usual jump to address 0.

For microcontrollers that have a UART, programming can be done through a PC
serial port using the bootloader. This makes programming even simpler,
particularly for PCs and laptops that do not have a parallel port. For those
without a serial port, a commercially available USB to serial adaptor can be
used.

Some tools developed for AVR programming are presented here. Similar tools are
available in both open source and commercial versions, including bootloaders, PC
programming control applications and hardware programmers. The tools described
here were constructed to add some useful features.

    *      AVR109 Bootloader adaptation
    *      Serial Programmer GUI software
    *	   Installation
    *      Serial to SPI Programmer Hardware
    * 	   Other Open Source Programmers

AVR Bootloader

Atmel's application note AVR109 describes a bootloader that uses the UART to
communicate FLASH programming instructions. They also provide some sample code
in C that allows the programming of FLASH, EEPROM, and lock bits. Note that the
AVR109 bootloader does not support the programming of fuses which for safety
cannot be changed by the bootloader. The communication protocol used is
compatible with the AVRPROG programming software. After loading this into the
bootloader section, the device can be configured so that on reset the program
counter starts at the bootloader, and programmer software can program the FLASH
memory.

The ATMEL example bootloader program proposes the use of a port pin on the
microcontroller to signal whether to execute the bootloader program or to jump
directly to the application. This allows a user to set a jumper on the
microcontroller card that pulls the pin low when programming of the FLASH is
needed.

The adaptation of this code described here, keeps it almost unchanged in the
interests of compatibility. By disabling the port pin test and excluding the
EEPROM programming section and the AVRPROG compatibility section, the code will
fit neatly into a 1K block of the bootloader FLASH section on
ATMega48/ATMega88/ATMega168 AVRs*. Disabling the port pin test means that on
reset the microcontroller always ends up in the bootloader program (if the
appropriate fuse bit has been set), and must be explicitly sent out again with a
bootloader exit command. For the moment this is acceptable as the device will
normally operate while connected to a PC serial port. For standalone operation a
pin will need to be made available to implement the port pin test, or the
appropriate fuse bit programmed to start the device on reset at location 0. In
the latter case the application firmware will need to manage a jump to the
bootloader when firmware update is required.

Although the AVR109 code is freely available, Atmel has not released it under a
clear open source licence. As such the modified code will not be reproduced
here. Later I may write my own bootloader or find another suitable source,
however it turns out to be difficult to produce more compact C code than that
provided in Atmel's sample. There are assembler versions of the bootloader
available (e.g. that by Herbert Dingfelder) that take up less space, however
assembler code is less readable and potentially less portable than a higher
level lenguage. The changes made to the code are quite reasonable and simple
(most are corrections to perceived minor deficiencies):

   1. Modify the block read and block write functions to add a compiler
directive REMOVE_FLASH_BYTE_SUPPORT around the FLASH programming section (be
careful of if-else clauses).
   2. Modify the block read and block write functions to add a compiler
directive REMOVE_EEPROM_BYTE_SUPPORT around the EEPROM programming section (be
extra careful of if-else clauses).
   3. Move the 'E' command outside of REMOVE_AVRPROG_SUPPORT so that we can get
out of the bootloader into the application.
   4. Add a compiler directive REMOVE_PROGPIN_TEST around the the port pin check
section. This must also be added at the end of the "for" loop where the jump to
the application is made.
   5. The preprocessor.sh script can be modified to generate an all-encompassing
defines.h file covering all microcontroller types, rather than using the
suggested cut and paste system. This makes use of the availability of the MMCU
variable in avr-gcc to identify the microcontroller type.

To allow the application to execute a jump into the bootloader I used an
instruction to enable the Watchdog Timer on a very short timeout to force the
MCU to be reset, the fuses being set to send it to the bootloader section. There
seemed to be a problem either with the (old) compiler or the AVR that a long
jump was not possible. This has the advantage that it does not require the exact
address of the bootloader to be set in the code, but of course prevents the
Watchdog Timer from being used for for other purposes. I then added code to
disable the Watchdog Timer at the start of the bootloader. This turned out to be
problematical as the ATMega88 and related microcontrollers use a two step
process to prevent accidental changes to the Watchdog Timer control registers.
Therefore the code below is specific to these MCUs. A small amount of work will
be needed to adapt it to other devices. The wdt.h header file provided with
avr-libc does not support this at the time of writing.

For the bootloader, clear all reset flags in the MCUSR, and then perform a write
to WDTCSR to set the WDCE and WDE bits. This enables changes to occur in a short
timeframe. Then immediately afterwards wipe the entire register to disable the
timer.

    asm("cli");
    asm("wdr");
    MCUSR = 0;
    WDTCSR |= (1 << WDCE) | (1 << WDE);
    WDTCSR = 0;

For the application, clear all reset flags in the MCUSR, and then perform a
write to WDTCSR to set the WDCE and WDE bits to enable changes. Then immediately
afterwards perform a write to WDTCSR to set the WDE bit to enable the timer for
system resets only, with the timeout period set at its smallest value (about
15ms).

    MCUSR = 0;
    WDTCSR = (1 << WDCE) | (1 << WDE);
    WDTCSR = (1 << WDE);

Programmer GUI Application

To support the above bootloader a GUI programmer for Linux was written in QT4
and C++ (see the documentation) . This provides basic hex file load and verify,
and some fuse/lock bit programming for selected devices (do not fiddle with
these settings until you have carefully read and understood the datasheets). The
programmer opens a serial port (which may need to be changed to match the target
system) and synchronizes with the device using no parity, 1 stop bit and 8 bit
data. A character 0xDD is sent and the character received back from the device
is checked for either a 0xDD or a "?". The latter indicates that the bootloader
may have been found. The 0xDD character relates to the packet protocol used in
the Acquisition project. If this is received, then a packet is sent to instruct
the device to jump to the bootloader. This can be removed if the packet protocol
is not needed, but that is not really necessary as the bootloader will just
reject anything unknown. If the bootloader is not present the device will
probably just reset itself. If neither character is received, the baud rate is
cyclically changed through a set of standard rates for a couple of cycles.

When the program has synchronized with the device, it gathers a swag of
information and presents the above window. Note that unlike avrdude it assumes
that a valid and working AVR device is connected. An Intel hex file can be
opened and will be immediately read and loaded to the application area of the
device FLASH.

The default baud rate to start with is 38400 baud. If this does not match that
of the device, then the program will cycle through standard baud rates between
2400 and 57600 baud until it finds the correct one. This process can be sped up
by changing the default baud rate to match that of the bootloader.

The program allows for turning off block mode, and also allows verification
without programming and vice-versa. Autoaddress currently does nothing so it is
disabled. There is a chip erase feature provided: normally the E button is
invisible until the checkbox has been selected to minimize accidental erasures.
An erase may be necessary if the lock bits have been set incorrectly. This
requires an external programmer. A bootloader will not reset the lock bits.

The GUI program relies on inserting delays into the serial communications to
adapt the speed of the PC to the bootloader or serial programmer. Also when
waiting for a response from the programmer, the program will timeout if this
takes too long. If problems are experienced while programming or verifying, it
may be that these delays need to be increased. This is particularly the case if
the clock speed used with the MCU is low. The GUI program was tested with 8MHz
MCU clock frequencies and 38400 baud communications.

Additional sections have been added to enable fuse and lock bits to be read and
changed for a limited number of devices. The program uses the device signature
bytes to identify the device uniquely, and the appropriate window is invoked.
This has only been partly tested so use it at your own risk.

The program also supports a limited command line feature to allow a much faster
non-GUI usage during intensive AVR development and testing work. The command
line options are:

-n	no GUI
-b	initial baudrate (baud)
-P	port (eg /dev/ttyUSB0)
-w	write hex file to device
-v	validate
-d	debug mode
-x	pass through serial comms on exit

Thus the command:

$ ./avrserialprog -n -b 19200 -P /dev/ttyUSB0 -w ../test/test.hex -v -d -x

will use the non-GUI mode, an initial baudrate of 19200 baud (only the standard
rates from 2400 to 115200 are supported), the given serial port (the one shown
uses a USB to serial adapter), to write the file shown with verification, to
give pages of useless debug information, and to pass through serial
communications at the end (so that the PC can communicate without removing the
programmer). The -x parameter is only useful if the programmer described below
is attached. In the GUI mode, the command line baud rate and port parameters can
be used to set those values in the program, but the other parameters will be
ignored.

Changes:

1/11/2010 Added a checkbox to the GUI to allow the program to pass through the
serial communications to the target device.

13/11/2010 Updated the AVR code to add more devices and to compile for the
ATTiny2313. The MCU needs to be changed in the Makefile and also at the top of
the .c file. The code now incorporates the serial link functions and is self
contained.

13/11/2010 The old AT90S2313 has been added but so far does not program
correctly. It doesn't support FLASH page programming and would require
additional code in the AVR for special handling. As code memory is too limited
it has been omitted from the AVR code (although it remains in the GUI). The
larger device ATTiny4313 would be needed to support the additional code.

14/11/2010 The AVR code now enables the SPI output ports only when programming
mode is entered, and reverts them to inputs as soon as programming mode quits.
This enables the sharing of the SPI ports for small devices when using
in-circuit programming.

13/10/2011 Some refactoring to move GUI specific code away from programmer
operations code. Addition of features to provide command line operation.

28/04/2012 Addition of an include for QFile in the header file (which seems
to have not been needed before). version 1.2Beta of qextserialport fails.
Nothing returned from availableBytes().

More information is provided at:

http://www.jiggerjuice.info/electronics/projects/serialprogrammer/firmware-bootloader.html

(c) K. Sarkies 19/07/2014

