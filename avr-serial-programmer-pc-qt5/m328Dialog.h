/*
Title:    Atmel Microcontroller Serial Port FLASH loader. M328 Lock/Fuse bits

The fuse bits are the same as the m88/m168 but in a different order (rabbits!!)
*/

/****************************************************************************
 *   Copyright (C) 2007 by Ken Sarkies                                      *
 *   ksarkies@trinity.asn.au                                                *
 *                                                                          *
 *   This file is part of avr-serial-prog                                   *
 *                                                                          *
 *   avr-serial-prog is free software; you can redistribute it and/or modify*
 *   it under the terms of the GNU General Public License as published by   *
 *   the Free Software Foundation; either version 2 of the License, or      *
 *   (at your option) any later version.                                    *
 *                                                                          *
 *   avr-serial-prog is distributed in the hope that it will be useful,     *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *   GNU General Public License for more details.                           *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with avr-serial-prog if not, write to the                        *
 *   Free Software Foundation, Inc.,                                        *
 *   51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA.              *
 ***************************************************************************/

#ifndef M328_DIALOG_H
#define M328_DIALOG_H
#define _TTY_POSIX_         // Need to tell qextserialport we are in POSIX

#include "ui/ui_m328Dialog.h"
#include <QSerialPort>
#include <QDialog>
#include <QCloseEvent>

//-----------------------------------------------------------------------------
/** @brief AVR Serial Programmer m328 Type Lock/Fuse bit Dialogue

This class provides a dialogue window
*/

class M328Dialog : public QDialog
{
    Q_OBJECT
public:
    M328Dialog(QSerialPort*, QWidget* parent = 0);
    ~M328Dialog();
    void setDefaults(uchar l, uchar e, uchar h, uchar f);
private slots:
    void on_closeButton_clicked();
    void on_memoryLockBox_currentIndexChanged(int memoryLock);
    void on_lockWriteButton_clicked();
    void on_extFuseWriteButton_clicked();
    void on_highFuseWriteButton_clicked();
    void on_fuseWriteButton_clicked();
private:
    QSerialPort* port;           //!< Serial port object pointer
    uchar lockBitsOriginal;
    uchar extFuseBitsOriginal;
    uchar highFuseBitsOriginal;
    uchar fuseBitsOriginal;
    char inBuffer[10];
// User Interface object
    Ui::M328Dialog m328DialogFormUi;
};

#endif
