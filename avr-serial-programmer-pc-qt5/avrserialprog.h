/*
Title:    Atmel Microcontroller Serial Port FLASH loader
*/

/****************************************************************************
 *   Copyright (C) 2007 by Ken Sarkies ksarkies@internode.on.net            *
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

#ifndef AVR_SERIAL_PROG_H
#define AVR_SERIAL_PROG_H
#define _TTY_POSIX_         // Need to tell qextserialport we are in POSIX

#include <QDialog>
#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QSerialPort>
#include "ui_avrserialprog.h"

//-----------------------------------------------------------------------------
/** @brief AVR Serial Programmer Control Window.

This class provides a window that appears when an AVRPROG bootloader has been
synchronized. The AVRPROG bootloader is described in Atmel's application note
AVR109 and supports a small set of commands for programming and checking the 
FLASH program ROM in an AVR microcontroller.

The window provides some simple choices for programming
- whether to program or just check the existing program.
- whether to check the program or not
- whether to use block transfers or individual word transfers

The programming and checking can sometimes be a bit unreliable, particularly if
the serial speed is a bit high, whence the AVR buffer may become overloaded for
long blocks. Using non-block transfers requires a lot more serial line traffic
and is therefore several times slower.

The entire programming and checking code is contained in this module.

The class is defined such that it can be incorporated at compile time into
other programs for the purpose of adding a firmware upload feature. The serial
port is defined in the main program. Thus a calling program must define the
port in a compatible way with the port used for its own operations.
*/

enum param {COMMANDLINEONLY,VERIFY,UPLOAD,DEBUG,READBLOCKMODE,WRITEBLOCKMODE,
            PASSTHROUGH,AUTOINCREMENTMODE};

class AvrSerialProg : public QDialog
{
    Q_OBJECT
public:
    AvrSerialProg(QString*, uint initialBaudrate,bool commandLine,
                        bool debug,QWidget* parent = 0);
    ~AvrSerialProg();
    bool success();
    QString error();
    void setParameter(param parameter, bool value);
    void printDetails();
    bool uploadHex(QString filename);
    bool downloadHex(QString filename,int startAddress, int endAddress);
    void quitProgrammer();
private slots:
    void on_debugModeCheckBox_stateChanged();
    void on_chipEraseCheckBox_stateChanged();
    void on_chipEraseButton_clicked();
    void on_cancelButton_clicked();
    void on_OKButton_clicked();
    void on_openFileButton_clicked();
    void on_readFileButton_clicked();
    void on_lockFuseButton_clicked();
private:
    bool getReadBlockMode();
    bool getWriteBlockMode();
    bool initializeProgrammer(uint initialBaudrate);
    void updateProgress(int progress);
    bool loadHexGUI(QString* errorMessage, QFile* file, const uchar memType);
    bool loadHexCore(bool upload, bool verify, QString* errorMessage, QFile* file,
                     const uchar memType);
    bool readHexGUI(QString* errorMessage, QFile* file, const uchar memType);
    bool readHexCore(uint startAddress, uint blockLength, QString* errorMessage,
                     QFile* file, const uchar memType);
    void hexDumpBuffer(const uchar* blockBuffer,
                       const uint blockLength,
                       const uint address);
    bool verifyPage(const uchar* blockBuffer,
                    const uint blockLength,
                    const uint address, const uchar memType);
    bool syncProgrammer(QSerialPort* port,const uchar baudrate);
    bool resyncProgrammer();
    bool setProgrammingMode();
    bool leaveProgrammingMode();
    bool getSignature(char* signature);
    bool getLockFuse(const uchar lockFuse, uchar& lockBits, uchar& fuseBits,
                                uchar& highFuseBits, uchar& extFuseBits);
    bool getAutoAddress(bool& autoAddress);
    bool getBlockSupport(bool& blockSupport, uint& pageSize);
    bool getVersion(QString& identifier);
    bool writePage(const uchar* blockBuffer,
                   const uint blockLength,
                   const uint address, const uchar memType);
    bool readPage(uchar* blockBuffer,
                   const uint blockLength,
                   const uint address, const uchar memType);
    bool sendAddress(const uint address);
    bool readPort(char* inBuffer, const int numBytes);
    int  checkCommand(const int expectedBytes);
    void sendCommand(const char command);
// User Interface object
    Ui::BootloaderDialog bootloaderFormUi;

    QSerialPort* port;          //!< Serial port object pointer
    bool synchronized;          //!< Synchronization status
    QString errorMessage;       //!< Messages for the calling application
    QDir saveDirectory;
    QString saveFile;
    QFile* outFile;
    QString identifier;         //!< AVR109 bootloader ID
    uchar lockFuse;             //!< Lock and Fuse capability byte
    uchar lockBits;
    uchar fuseBits;
    uchar highFuseBits;
    uchar extFuseBits;
    char signatureArray[3];
    bool autoincrement;         //!< If address is autoincremented
    bool blockSupport;          //!< If blocks of data can be sent at once
    uint pageSize;              //!< Size of FLASH pages for writing.
    QString deviceType;         //!< Symbolic microcontroller type name
    uint partType;              //!< Part Type symbol
// Control parameters
    bool verify;
    bool upload;
    bool commandLineOnly;
    bool debugMode;
    bool readBlockMode;
    bool writeBlockMode;
    bool autoincrementMode;
    bool passThrough;
};

#endif
