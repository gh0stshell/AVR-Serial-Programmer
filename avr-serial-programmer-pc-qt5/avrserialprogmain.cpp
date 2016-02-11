/**
@mainpage Atmel AVR Microcontroller Serial Port FLASH Programmer
@version 1.0
@author Ken Sarkies (www.jiggerjuice.net)
@date 28 April 2012

This describes the structure and detail of a PC based user interface program
to program an AVR microcontroller through a serial (USB) link. The development
documentation is supplemented by a user manual.

The software is used for communicating with the Bootloader and the Serial
Programmer described as part of this project.

The Serial Programmer must follow the AVRPROG command interface described in
the Atmel application note AVR109.

The bootloader is a small program residing in the AVR FLASH memory. The AVR
must have been setup so that on reset the device begins execution at the start
of the bootloader. The bootloader must follow the AVRPROG command interface
described in the Atmel application note AVR109. The bootloader does not
pre-exist in the AVR devices, and must be added separately using a low level
programmer, typically one that uses the SPI interface.

The serial interface is a USB port to which a USB to Serial adaptor is connected.

@note
Compiler: gcc (Ubuntu)
@note
Uses: Qt version 5.2.1

@todo Add a feature to allow the serial interface to be selected and tested.
@todo Add support for s-record (which can program memories > 64K).
*/
/****************************************************************************
 *   Copyright (C) 2007 by Ken Sarkies                                      *
 *   ksarkies@trinity.asn.au                                                *
 *                                                                          *
 *   This file is part of serial-programmer                                 *
 *                                                                          *
 *   serial-programmer is free software; you can redistribute it and/or     *
 *   modify it under the terms of the GNU General Public License as         *
 *   published bythe Free Software Foundation; either version 2 of the      *
 *   License, or (at your option) any later version.                        *
 *                                                                          *
 *   serial-programmer is distributed in the hope that it will be useful,   *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *   GNU General Public License for more details.                           *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with serial-programmer if not, write to the                      *
 *   Free Software Foundation, Inc.,                                        *
 *   51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA.              *
 ***************************************************************************/

#include <QApplication>
#include <QMessageBox>
#include <QDebug>
#include <unistd.h>
#include "avrserialprog.h"
#include "serialport.h"

#define SERIAL_PORT "ttyUSB0"

//-----------------------------------------------------------------------------
/** @brief Serial Programmer Main Program

This creates a serial port object and a programming window object. The latter
first attempts to synchronize with the bootloader in the microcontroller. If
this is successful, the window is opened for use.
*/

int main(int argc,char ** argv)
{
    QString serialPort = SERIAL_PORT;
    int c;
    uint initialBaudrate = 3; //!< Baudrate index to start searching
    int baudParm;
    bool commandLineOnly = false;
    bool loadHex = false;
    bool readHex = false;
    bool debug = false;
    bool verify = false;
    bool passThrough = false;
    bool ok;
    uint startAddress = 0;
    uint endAddress = 0xFFFF;
    QString filename;

    opterr = 0;
    while ((c = getopt (argc, argv, "w:r:s:e:P:ndvxb:")) != -1)
    {
        switch (c)
        {
        case 'w':
            loadHex = true;
            filename = QString(optarg);
            break;
        case 'r':
            readHex = true;
            filename = QString(optarg);
            break;
        case 's':
            startAddress = QString(optarg).toInt(&ok,16);
            break;
        case 'e':
            endAddress = QString(optarg).toInt(&ok,16);
            break;
        case 'P':
            serialPort = optarg;
            break;
        case 'n':
            commandLineOnly = true;
            break;
        case 'd':
            debug = true;
            break;
        case 'v':
            verify = true;
            break;
        case 'x':
            passThrough = true;
            break;
        case 'b':
            baudParm = atoi(optarg);
            switch (baudParm)
            {
            case 2400: initialBaudrate=0;break;
            case 4800: initialBaudrate=1;break;
            case 9600: initialBaudrate=2;break;
            case 19200: initialBaudrate=3;break;
            case 38400: initialBaudrate=4;break;
            case 57600: initialBaudrate=5;break;
            case 115200: initialBaudrate=6;break;
            default:
                fprintf (stderr, "Invalid Baudrate %i.\n", baudParm);
                return false;
            }
            break;
        case '?':
            if (optopt == 'P')
                fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            else if (isprint (optopt))
                fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf (stderr,"Unknown option character `\\x%x'.\n",optopt);
            default: return false;
        }
    }

    QApplication application(argc,argv);
    AvrSerialProg serialProgrammer(&serialPort,initialBaudrate,commandLineOnly,debug);
    if (! commandLineOnly)
    {
        if (serialProgrammer.success())
        {
            serialProgrammer.show();
            return application.exec();
        }
        else
        {
            QMessageBox::critical(0,"Bootloader Problem",
                  QString("Couldn't Contact Bootloader\n%1")
                                      .arg(serialProgrammer.error()));
             return true;
        }
    }
/* Command line only actions */
    else
    {
        serialProgrammer.printDetails();
        serialProgrammer.setParameter(PASSTHROUGH,passThrough);
        serialProgrammer.setParameter(UPLOAD,loadHex);
        serialProgrammer.setParameter(VERIFY,verify);
        if (loadHex && readHex) qDebug() << "Read and write both specified";
        else if (readHex && (!ok || (startAddress > endAddress)))
            qDebug() << "Invalid hexadecimal address";
        else
        {
            if (loadHex) serialProgrammer.uploadHex(filename);
            if (readHex) serialProgrammer.downloadHex(filename,startAddress,endAddress);
            qDebug() << "Leaving Normally";
        }
        serialProgrammer.quitProgrammer();
    }
    return true;

}
