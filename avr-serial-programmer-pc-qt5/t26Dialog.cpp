/*
@brief                        T26 Type Lock/Fuse bits

@details Open a dialogue window with lock and fuse bits available for editing and
programming. This applies to the ATTiny261/461/861.

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

#include "t26Dialog.h"
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

T26Dialog::T26Dialog(QSerialPort* p, QWidget* parent) : QDialog(parent)
{
    port = p;
// Build the User Interface display from the Ui class in ui_mainwindowform.h
    t26DialogFormUi.setupUi(this);
}

T26Dialog::~T26Dialog()
{
//    port->close();
}

//-----------------------------------------------------------------------------
/** @brief Setup the widgets to a default state

The default values for the various lock/fuse bits are set and the widgets set to indicate these.
*/

void T26Dialog::setDefaults(uchar l, uchar h, uchar f)
{
// Set the lock bit widgets to represent the current status.
    lockBitsOriginal = l;
    uint memoryLockOriginal = (lockBitsOriginal&0x03);
    if (memoryLockOriginal == 0) t26DialogFormUi.memoryLockBox->setCurrentIndex(2);
    else if (memoryLockOriginal == 2) t26DialogFormUi.memoryLockBox->setCurrentIndex(1);
    else if (memoryLockOriginal == 3) t26DialogFormUi.memoryLockBox->setCurrentIndex(0);
// Set the high fuse bit widgets to represent the current status.
    highFuseBitsOriginal = h;
    t26DialogFormUi.brownoutBox->setCurrentIndex(0x03-(highFuseBitsOriginal&0x03));
    if ((highFuseBitsOriginal&0x08)>0)
        t26DialogFormUi.preserveEEPROMBox->setChecked(false);
    else
        t26DialogFormUi.preserveEEPROMBox->setChecked(true);
    if ((highFuseBitsOriginal&0x20)>0)
        t26DialogFormUi.enableSerialBox->setChecked(false);
    else
        t26DialogFormUi.enableSerialBox->setChecked(true);
    if ((highFuseBitsOriginal&0x80)>0)
        t26DialogFormUi.resetDisableBox->setChecked(false);
    else
        t26DialogFormUi.resetDisableBox->setChecked(true);
// Set the fuse bit widgets to represent the current status.
    fuseBitsOriginal = f;
    if ((fuseBitsOriginal&0x01)>0)
        t26DialogFormUi.clockSourceBox->setChecked(true);
    else
        t26DialogFormUi.clockSourceBox->setChecked(false);
    if ((fuseBitsOriginal&0x02)>0)
        t26DialogFormUi.clockSourceBox_2->setChecked(true);
    else
        t26DialogFormUi.clockSourceBox_2->setChecked(false);
    if ((fuseBitsOriginal&0x04)>0)
        t26DialogFormUi.clockSourceBox_3->setChecked(true);
    else
        t26DialogFormUi.clockSourceBox_3->setChecked(false);
    if ((fuseBitsOriginal&0x08)>0)
        t26DialogFormUi.clockSourceBox_4->setChecked(true);
    else
        t26DialogFormUi.clockSourceBox_4->setChecked(false);
    if ((fuseBitsOriginal&0x10)>0)
        t26DialogFormUi.startupTimeBox->setChecked(true);
    else
        t26DialogFormUi.startupTimeBox->setChecked(false);
    if ((fuseBitsOriginal&0x20)>0)
        t26DialogFormUi.startupTimeBox_2->setChecked(true);
    else
        t26DialogFormUi.startupTimeBox_2->setChecked(false);
    if ((fuseBitsOriginal&0x40)>0)
        t26DialogFormUi.clockOptBox->setChecked(false);
    else
        t26DialogFormUi.clockOptBox->setChecked(true);
    if ((fuseBitsOriginal&0x80)>0)
        t26DialogFormUi.clockPLLBox->setChecked(false);
    else
        t26DialogFormUi.clockPLLBox->setChecked(true);
}

//-----------------------------------------------------------------------------
/** @brief Close when button is activated.

No further action is taken, and the device may remain in the bootloader.
*/

void T26Dialog::on_closeButton_clicked()
{
    reject();
}

//-----------------------------------------------------------------------------
/** @brief Do some checks and write the selected lock bits.

Interpret the selection and build the lock bits.
The first two bits control memory lock (only three settings).
The next two determine application memory locking.
The next two determine boot memory locking.
The last two are reserved and unprogrammed (set to 1's).
*/

void T26Dialog::on_lockWriteButton_clicked()
{
    uchar lockBits = 0;                             // All read/write locked including boot lock bits
    int memoryLock = t26DialogFormUi.memoryLockBox->currentIndex();
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
/** @brief Act on changes to the memoryLock ComboBox

Check the new setting and disable or enable the other boxes accordingly.
A QFrame can be disabled in its entirety.
*/

void T26Dialog::on_memoryLockBox_currentIndexChanged(int memoryLock)
{
    if (memoryLock != 0)
    {
        t26DialogFormUi.highFuseFrame->setEnabled(false);
        t26DialogFormUi.fuseFrame->setEnabled(false);
    }
    else
    {
        t26DialogFormUi.highFuseFrame->setEnabled(true);
        t26DialogFormUi.fuseFrame->setEnabled(true);
    }
}

//-----------------------------------------------------------------------------
/** @brief Do some checks and write the selected high fuse bits.

(note the programmed state is 0, which is boolean false)
Interpret the selection and build the high fuse bits.
0. Brownout enable
1. Brownout voltage level threshold: 0=2.7V, 1=4.0V
2. EEPROM programming is preserved through a chip erase.
3. Enable serial programming and data download.
4. Disable external reset.
*/

void T26Dialog::on_highFuseWriteButton_clicked()
{
    uchar highFuseBits = 0;
    int bodLevel = t26DialogFormUi.brownoutBox->currentIndex();
    if (bodLevel == 1) highFuseBits+=0x01;
    else if (bodLevel == 2) highFuseBits+=0x03;
    if (!t26DialogFormUi.preserveEEPROMBox->isChecked()) highFuseBits+=0x04;
    if (!t26DialogFormUi.enableSerialBox->isChecked()) highFuseBits+=0x08;
    if (!t26DialogFormUi.resetDisableBox->isChecked()) highFuseBits+=0x10;
    if (highFuseBits != highFuseBitsOriginal)
    {
        port->putChar('n');                         // Issue a high fuse byte write request
        port->putChar(highFuseBits);
        port->read(inBuffer,1);                     // Pull in and discard response
    }
}

//-----------------------------------------------------------------------------
/** @brief Do some checks and write the selected fuse bits.

(note the programmed state is 0, which is boolean false)
Interpret the selection and build the high fuse bits.
The first four bits set the clock source selection. This is complex so the user must determine this.
The next two bits determine the startup time selection, which vary according to the clock source.
6. Clock is output on port PB0.
7. Divide the clock by 8.
*/

void T26Dialog::on_fuseWriteButton_clicked()
{
    uint fuseBits = 0;
    if (t26DialogFormUi.clockSourceBox->isChecked()) fuseBits+=0x01;
    if (t26DialogFormUi.clockSourceBox_2->isChecked()) fuseBits+=0x02;
    if (t26DialogFormUi.clockSourceBox_3->isChecked()) fuseBits+=0x04;
    if (t26DialogFormUi.clockSourceBox_4->isChecked()) fuseBits+=0x08;
    if (t26DialogFormUi.startupTimeBox->isChecked()) fuseBits+=0x10;
    if (t26DialogFormUi.startupTimeBox_2->isChecked()) fuseBits+=0x20;
    if (!t26DialogFormUi.clockOptBox->isChecked()) fuseBits+=0x40;
    if (!t26DialogFormUi.clockPLLBox->isChecked()) fuseBits+=0x80;
    if (fuseBits != fuseBitsOriginal)
    {
        port->putChar('f');                         // Issue a fuse byte write request
        port->putChar(fuseBits);
        port->read(inBuffer,1);                     // Pull in and discard response
    }
}
//-----------------------------------------------------------------------------

