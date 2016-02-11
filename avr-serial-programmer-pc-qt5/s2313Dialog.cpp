/*
@brief                        S2313 Type Lock/Fuse bits

@details Open a dialogue window with lock and fuse bits available for editing and
programming. This applies to the AT90S2313.

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

#include "s2313Dialog.h"
#include <QSerialPort>
#include <QString>
#include <QLabel>
#include <QMessageBox>
#include <QCloseEvent>
#include <QDebug>
#include <QBasicTimer>
#include <cstdlib>
#include <iostream>

//-----------------------------------------------------------------------------
/** Constructor

@param p Serial Port object pointer
@param parent Parent widget.
*/

S2313Dialog::S2313Dialog(QSerialPort* p, QWidget* parent) : QDialog(parent)
{
    port = p;
// Build the User Interface display from the Ui class in ui_mainwindowform.h
    s2313DialogFormUi.setupUi(this);
}

S2313Dialog::~S2313Dialog()
{
//    port->close();
}

//-----------------------------------------------------------------------------
/** @brief Setup the widgets to a default state

The default values for the various lock/fuse bits are set and the widgets set to indicate these.
*/

void S2313Dialog::setDefaults()
{
// Set the lock bit widgets to represent the current status. The device doesn't allow
// reading lock bits, so set to the default "no locks".
    lockBitsOriginal = 0x03;
    uint memoryLockOriginal = (lockBitsOriginal&0x03);
    if (memoryLockOriginal == 0) s2313DialogFormUi.memoryLockBox->setCurrentIndex(2);
    else if (memoryLockOriginal == 2) s2313DialogFormUi.memoryLockBox->setCurrentIndex(1);
    else if (memoryLockOriginal == 3) s2313DialogFormUi.memoryLockBox->setCurrentIndex(0);
}

//-----------------------------------------------------------------------------
/** @brief Close when button is activated.

No further action is taken, and the device may remain in the bootloader.
*/

void S2313Dialog::on_closeButton_clicked()
{
    reject();
}

//-----------------------------------------------------------------------------
/** @brief Do some checks and write the selected lock bits.

Interpret the selection and build the lock bits.
The first two bits control memory lock (only three settings).
The rest are reserved and unprogrammed (set to 1's).
*/

void S2313Dialog::on_lockWriteButton_clicked()
{
    uchar lockBits = 0;                             // All read/write locked including boot lock bits
    int memoryLock = s2313DialogFormUi.memoryLockBox->currentIndex();
    if (memoryLock == 0) lockBits = 3;              // Set to no memory locks
    else if (memoryLock == 1) lockBits = 2;         // Disable Write to FLASH, EEPROM, Fuses
    if (lockBits != lockBitsOriginal)
    {
        port->putChar('l');                         // Issue a lock byte write request
        port->putChar(lockBits);
        port->read(inBuffer,1);                     // Pull in and discard response
    }
}

//-----------------------------------------------------------------------------

