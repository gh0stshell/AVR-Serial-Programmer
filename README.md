AVR Serial Programmer
---------------------

This project is a small programmer for AVR microcontrollers using a serial
connection. The circuit has a feature that allows the firmware to be loaded to
the target microcontroller, and the serial line to be passed through to
the target without manual intervention. A jumper setting allows for the
programmer firmware to be updated. The microcontroller used is now the
ATTiny4313. The original AT90S2313 is obsolete and the ATTiny2313 is now too
limited.

The firmware manages the programming of an expandable range of AVR
microcontrollers, FLASH, EEPROM and fuse bits, using the SPI port of the target
with the reset pin asserted. Versions of firmware are provided for the
ATTiny2313 and ATTiny4313. The former is deprecated. The latter is needed to
handle the increasing number of AVR target types.

A GUI is provided. This reads the AVR fuse bits and signature bits and adapts
the GUI configuration to the detected microcontroller type.

(c) K. Sarkies 19/07/2014

