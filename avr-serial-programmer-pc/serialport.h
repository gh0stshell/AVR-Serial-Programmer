/*
Title:    Serial Port Subclass
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

#ifndef ACQSERIALPORT_H
#define ACQSERIALPORT_H
#define _TTY_POSIX_         // Need to tell qextserialport we are in POSIX

#include <qextserialport.h>
#include <QString>

//-----------------------------------------------------------------------------
/** @brief Serial Port extensions to QextSerialPort.

@details This class is a subclass of QExtSerialPort to allow the initialization
of the serial interface by searching for a valid response at different bitrates.
This allows the program to be used without the need (in most cases) for
setting the bitrate manually. It can fail if the target device is not using
the expected standard set of bitrates.*/

class SerialPort : public QextSerialPort
{
public:
    SerialPort();
    SerialPort(const QString & name);
    ~SerialPort();
    bool initPort(const uchar baudrate,const long timeout);
};

#endif
