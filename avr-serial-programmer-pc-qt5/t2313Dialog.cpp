/*
@brief                        T2313 Type Lock/Fuse bits

@details Open a dialogue window with lock and fuse bits available for editing and
programming. This applies to the ATTiny2313.

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

#include "t2313Dialog.h"
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

T2313Dialog::T2313Dialog(QSerialPort* p, QWidget* parent) : QDialog(parent)
{
    port = p;
// Build the User Interface display from the Ui class in ui_mainwindowform.h
    t261DialogFormUi.setupUi(this);
}

T2313Dialog::~T2313Dialog()
{
//    port->close();
}

//-----------------------------------------------------------------------------
/** @brief Setup the widgets to a default state

The default values for the various lock/fuse bits are set and the widgets set to indicate these.
*/

void T2313Dialog::setDefaults(uchar l, uchar e, uchar h, uchar f)
{
// Set the lock bit widgets to represent the current status.
    lockBitsOriginal = l;
    uint memoryLockOriginal = (lockBitsOriginal&0x03);
    if (memoryLockOriginal == 0) t261DialogFormUi.memoryLockBox->setCurrentIndex(2);
    else if (memoryLockOriginal == 2) t261DialogFormUi.memoryLockBox->setCurrentIndex(1);
    else if (memoryLockOriginal == 3) t261DialogFormUi.memoryLockBox->setCurrentIndex(0);
// Set the extended fuse bit widgets to represent the current status.
    extFuseBitsOriginal = e;
    if ((extFuseBitsOriginal&0x01)>0)
        t261DialogFormUi.selfProgCheck->setChecked(false);
    else
        t261DialogFormUi.selfProgCheck->setChecked(true);
// Set the high fuse bit widgets to represent the current status.
    highFuseBitsOriginal = h;
    t261DialogFormUi.brownoutBox->setCurrentIndex(0x03-(highFuseBitsOriginal&0x03));
    if ((highFuseBitsOriginal&0x08)>0)
        t261DialogFormUi.preserveEEPROMBox->setChecked(false);
    else
        t261DialogFormUi.preserveEEPROMBox->setChecked(true);
    if ((highFuseBitsOriginal&0x10)>0)
        t261DialogFormUi.watchdogOnBox->setChecked(false);
    else
        t261DialogFormUi.watchdogOnBox->setChecked(true);
    if ((highFuseBitsOriginal&0x20)>0)
        t261DialogFormUi.enableSerialBox->setChecked(false);
    else
        t261DialogFormUi.enableSerialBox->setChecked(true);
    if ((highFuseBitsOriginal&0x40)>0)
        t261DialogFormUi.debugWIREBox->setChecked(false);
    else
        t261DialogFormUi.debugWIREBox->setChecked(true);
    if ((highFuseBitsOriginal&0x80)>0)
        t261DialogFormUi.resetDisableBox->setChecked(false);
    else
        t261DialogFormUi.resetDisableBox->setChecked(true);
// Set the fuse bit widgets to represent the current status.
    fuseBitsOriginal = f;
    if ((fuseBitsOriginal&0x01)>0)
        t261DialogFormUi.clockSourceBox->setChecked(true);
    else
        t261DialogFormUi.clockSourceBox->setChecked(false);
    if ((fuseBitsOriginal&0x02)>0)
        t261DialogFormUi.clockSourceBox_2->setChecked(true);
    else
        t261DialogFormUi.clockSourceBox_2->setChecked(false);
    if ((fuseBitsOriginal&0x04)>0)
        t261DialogFormUi.clockSourceBox_3->setChecked(true);
    else
        t261DialogFormUi.clockSourceBox_3->setChecked(false);
    if ((fuseBitsOriginal&0x08)>0)
        t261DialogFormUi.clockSourceBox_4->setChecked(true);
    else
        t261DialogFormUi.clockSourceBox_4->setChecked(false);
    if ((fuseBitsOriginal&0x10)>0)
        t261DialogFormUi.startupTimeBox->setChecked(true);
    else
        t261DialogFormUi.startupTimeBox->setChecked(false);
    if ((fuseBitsOriginal&0x20)>0)
        t261DialogFormUi.startupTimeBox_2->setChecked(true);
    else
        t261DialogFormUi.startupTimeBox_2->setChecked(false);
    if ((fuseBitsOriginal&0x40)>0)
        t261DialogFormUi.clockOutBox->setChecked(false);
    else
        t261DialogFormUi.clockOutBox->setChecked(true);
    if ((fuseBitsOriginal&0x80)>0)
        t261DialogFormUi.clockDivide8Box->setChecked(false);
    else
        t261DialogFormUi.clockDivide8Box->setChecked(true);
}

//-----------------------------------------------------------------------------
/** @brief Close when button is activated.

No further action is taken, and the device may remain in the bootloader.
*/

void T2313Dialog::on_closeButton_clicked()
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

void T2313Dialog::on_lockWriteButton_clicked()
{
    uchar lockBits = 0;                             // All read/write locked including boot lock bits
    int memoryLock = t261DialogFormUi.memoryLockBox->currentIndex();
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

void T2313Dialog::on_memoryLockBox_currentIndexChanged(int memoryLock)
{
    if (memoryLock != 0)
    {
        t261DialogFormUi.extFuseFrame->setEnabled(false);
        t261DialogFormUi.highFuseFrame->setEnabled(false);
        t261DialogFormUi.fuseFrame->setEnabled(false);
    }
    else
    {
        t261DialogFormUi.extFuseFrame->setEnabled(true);
        t261DialogFormUi.highFuseFrame->setEnabled(true);
        t261DialogFormUi.fuseFrame->setEnabled(true);
    }
}

//-----------------------------------------------------------------------------
/** @brief Do some checks and write the selected extended fuse bits.

Interpret the selection and build the extended fuse bits.
The first bit sets how the boot vector works, high selects 0000 and low sets the bootloader.
The next two determine bootloader memory size.
*/

void T2313Dialog::on_extFuseWriteButton_clicked()
{
    uchar extFuseBits = 0;
    if (!t261DialogFormUi.selfProgCheck->isChecked()) extFuseBits+=1;
    if (extFuseBits != extFuseBitsOriginal)
    {
        port->putChar('q');                         // Issue a extended fuse byte write request
        port->putChar(extFuseBits);
        int numBytes = 0;
        while(numBytes <= 0) numBytes = port->bytesAvailable();
        port->read(inBuffer,1);                     // Pull in and discard response
    }
}

//-----------------------------------------------------------------------------
/** @brief Do some checks and write the selected high fuse bits.

(note the programmed state is 0, which is boolean false)
Interpret the selection and build the high fuse bits.
The first three bits set the brownout voltage level threshold: 0=disabled,l 1=1.8V, 2=2.7V, 3=4.3V
2. The third bit of brownout threshold is reserved (always 1).
3. EEPROM programming is preserved through a chip erase.
4. Watchdog timer is always on.
5. Enable serial programming and data download.
6. Enable debugWIRE.
7. Disable external reset.
*/

void T2313Dialog::on_highFuseWriteButton_clicked()
{
    int bodLevel = t261DialogFormUi.brownoutBox->currentIndex();
    uchar highFuseBits = 0;
    highFuseBits +=(0x07-bodLevel)<<1;        // Compute BOD Level settings (3-bodlevel+bit3)
    if (!t261DialogFormUi.preserveEEPROMBox->isChecked()) highFuseBits+=0x40;
    if (!t261DialogFormUi.watchdogOnBox->isChecked()) highFuseBits+=0x10;
    if (!t261DialogFormUi.enableSerialBox->isChecked()) highFuseBits+=0x20;
    if (!t261DialogFormUi.debugWIREBox->isChecked()) highFuseBits+=0x80;
    if (!t261DialogFormUi.resetDisableBox->isChecked()) highFuseBits+=0x01;
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

void T2313Dialog::on_fuseWriteButton_clicked()
{
    uint fuseBits = 0;
    if (t261DialogFormUi.clockSourceBox->isChecked()) fuseBits+=0x01;
    if (t261DialogFormUi.clockSourceBox_2->isChecked()) fuseBits+=0x02;
    if (t261DialogFormUi.clockSourceBox_3->isChecked()) fuseBits+=0x04;
    if (t261DialogFormUi.clockSourceBox_4->isChecked()) fuseBits+=0x08;
    if (t261DialogFormUi.startupTimeBox->isChecked()) fuseBits+=0x10;
    if (t261DialogFormUi.startupTimeBox_2->isChecked()) fuseBits+=0x20;
    if (!t261DialogFormUi.clockOutBox->isChecked()) fuseBits+=0x40;
    if (!t261DialogFormUi.clockDivide8Box->isChecked()) fuseBits+=0x80;
    if (fuseBits != fuseBitsOriginal)
    {
        port->putChar('f');                         // Issue a fuse byte write request
        port->putChar(fuseBits);
        port->read(inBuffer,1);                     // Pull in and discard response
    }
}
//-----------------------------------------------------------------------------

