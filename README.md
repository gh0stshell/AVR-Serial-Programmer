AVR Serial Programmer
---------------------

This project is a small programmer for AVR microcontrollers using an RS232
serial connection. The circuit has a feature that allows the firmware to be
loaded to the target microcontroller, and the serial line to be passed through
to the target without manual intervention. A jumper setting allows for the
programmer firmware to be updated. The microcontroller used in the programmer
hardware is now the ATTiny4313. The original AT90S2313 is obsolete and the
ATTiny2313 is now too limited.

The firmware manages the programming of an expandable range of AVR
microcontrollers, FLASH, EEPROM and fuse bits, using the SPI port of the target
with the reset pin asserted. Versions of firmware are provided for the
ATTiny2313 and ATTiny4313. The former is deprecated. The latter is needed to
handle the increasing number of AVR target types.

A Linux GUI is provided. This reads the AVR fuse bits and signature bits and
adapts the GUI configuration to the detected microcontroller type. The GUI may
be used directly with a bootloader following the AVR109 command interface.

* Connect the 2-pin 5V DC power, noting the polarity is respected.
* Connect a 4-wire serial cable to the PC either directly if the PC has a serial
port, or via a serial-USB adapter. The default serial port is /dev/ttyUSB0
appropriate for the adapter. The -P command line switch allows another port to
be defined, such as /dev/ttyS0 for the PC's serial port.
* Connect the 6-pin programming cable to the target.

The GUI when invoked should show the target AVR processor type plus a number of
additional details. If the serial port is incorrect the GUI will try out a
number of baud rates and close.

Take care when changing the lock/fuse bits as this can brick the processor if
done incorrectly.

More information is provided at:

http://www.jiggerjuice.info/electronics/projects/serialprogrammer/firmware-bootloader.html

(c) K. Sarkies 19/07/2014

