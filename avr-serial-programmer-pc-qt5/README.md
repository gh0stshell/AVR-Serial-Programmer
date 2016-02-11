AVR Serial Programmer GUI
=========================

The Serial Programmer GUI is intended for use with any AVRPROG compatible Serial
Programmer or with AVR microcontrollers that have an AVR109 compatible
bootloader installed.

More information is provided at:

http://www.jiggerjuice.info/electronics/projects/serialprogrammer/firmware-bootloader.html

(c) K. Sarkies 19/07/2014

Conversion to QT5
-----------------

1.  Change all obsolete QString methods toAscii() to toLatin1()

2.  Add to .pro file
    QT += serialport
    QT += widgets

3.  To use QSerialPort in Ubuntu, need to install libqt5serialport5-dev and
    libudev-dev.

4.  Add the following instead of qextserialport references:
    #include <QSerialPort>
    #include <QSerialPortInfo>

5.  The only changes needed were to open the port, and set serial parameters. To
    change baudrate there was no need to close and reopen the port. A direct
    change is acceptable.

CHANGES
-------

1.  Convert to QT5 and replace qextserialport with QSerialPort.

