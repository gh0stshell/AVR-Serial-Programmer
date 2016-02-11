/*
Title:    Atmel Microcontroller Serial Port FLASH loader. S2313 Lock/Fuse bits
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

#ifndef S2313_DIALOG_H
#define S2313_DIALOG_H
#define _TTY_POSIX_         // Need to tell qextserialport we are in POSIX

#include "ui/ui_s2313Dialog.h"
#include <QSerialPort>
#include <QDialog>
#include <QCloseEvent>

//-----------------------------------------------------------------------------
/** @brief AVR Serial Programmer s2313 Type Lock/Fuse bit Dialogue

This class provides a dialogue window
*/

class S2313Dialog : public QDialog
{
    Q_OBJECT
public:
    S2313Dialog(QSerialPort*, QWidget* parent = 0);
    ~S2313Dialog();
    void setDefaults();
private slots:
    void on_closeButton_clicked();
    void on_lockWriteButton_clicked();
private:
    QSerialPort* port;           //!< Serial port object pointer
    uchar lockBitsOriginal;
    char inBuffer[10];
// User Interface object
    Ui::S2313Dialog s2313DialogFormUi;
};

#endif
