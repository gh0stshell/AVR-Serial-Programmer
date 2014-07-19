/**
@brief        Atmel Microcontroller Serial Port FLASH loader

@detail Communicate with a serial port programmer, either AVR109 bootloader or
other stand-alone programmer using the AVRProg protocol.

This application connects to the programmer to identify it and collect various
important information about the device being programmed. It then allows a hex
file to be opened and transmitted for programming, using block or single
mode, and with or without verification.

17/2/2010 Removed test for end of Flash memory (as it was set for an 8K device)
          Corrected the blocksize conversion from uchar to uint in 'b'
6/9/2010  In syncProgrammer, calls to port->bytesAvailable() returned a (signed)
          qint64, but this was assigned to a uint numBytes. Sometimes a -1 was
          returned resulting in failure to sync. Added int checkBytes for the
          tests and assigned this to numBytes when needed. Problem showed up
          after Ubuntu upgrade.
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

// Specify an intercharacter timeout when receiving incoming communications
#define TIMEOUTCOUNT 100000

#include "avrserialprog.h"
#include "m328Dialog.h"
#include "m88Dialog.h"
#include "m48Dialog.h"
#include "m8535Dialog.h"
#include "m16Dialog.h"
#include "t261Dialog.h"
#include "t26Dialog.h"
#include "t2313Dialog.h"
#include "s2313Dialog.h"
#include "serialport.h"
#include <QApplication>
#include <QString>
#include <QLineEdit>
#include <QLabel>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextEdit>
#include <QCloseEvent>
#include <QDebug>
#include <QBasicTimer>
#include <cstdlib>
#include <unistd.h>
#include <iostream>

#define IDLE_CHAR 0xDD
#define SYNC_CHAR 0x67
#define EOM_CHAR 0x03

//-----------------------------------------------------------------------------
/* Supported devices arrays.
Type is a single digit indicator of the lock/fuse byte structure.
EPage is the number of bytes in an EEPROM page.
Busy is a boolean indicator that a busy status command is provided.
Lock and Fuse support is given by a bitwise quantity.
0 = Lock Read
1 = Fuse Read
2 = High Fuse Read
3 = Extended Fuse Read
4 = Lock Write
5 = Fuse Write
6 = High Fuse Write
7 = Extended Fuse Write
*/

#define NUMPARTS 17
const uint part[NUMPARTS][6] = {
/* Sig 2, Sig 3,   Type, EPage, Busy, Lock/Fuse */
{   0x91,  0x01,   12313, 0,   FALSE,  0x10     },  // AT90S2313
{   0x91,  0x0B,   261,   4,   TRUE,   0xFF     },  // ATTiny24
{   0x91,  0x09,   26,    0,   FALSE,  0x77     },  // ATTiny26
{   0x91,  0x0A,   2313,  4,   TRUE,   0xFF     },  // ATTiny2313
{   0x91,  0x0C,   261,   4,   TRUE,   0xFF     },  // ATTiny261
{   0x92,  0x0D,   2313,  4,   TRUE,   0xFF     },  // ATTiny4313
{   0x92,  0x07,   261,   4,   TRUE,   0xFF     },  // ATTiny44
{   0x92,  0x05,   48,    4,   TRUE,   0xFF     },  // ATMega48
{   0x92,  0x08,   261,   4,   TRUE,   0xFF     },  // ATTiny461
{   0x93,  0x0C,   261,   4,   TRUE,   0xFF     },  // ATTiny84
{   0x93,  0x08,   8535,  0,   FALSE,  0x77     },  // ATMega8535
{   0x93,  0x0A,   88,    4,   TRUE,   0xFF     },  // ATMega88
{   0x93,  0x0D,   261,   4,   TRUE,   0xFF     },  // ATTiny861
{   0x94,  0x03,   16,    4,   TRUE,   0x77     },  // ATMega16
{   0x94,  0x06,   88,    4,   TRUE,   0xFF     },  // ATMega168
{   0x95,  0x0F,   328,   4,   TRUE,   0xFF     },  // ATMega328
{   0x95,  0x02,   16,    0,   FALSE,  0x77     }   // ATMega32
};
const QString partName[NUMPARTS] = {
"AT90S2313",
"ATTiny24",
"ATTiny26",
"ATTiny2313",
"ATTiny261",
"ATTiny4313",
"ATTiny44",
"ATMega48",
"ATTiny461",
"ATTiny84",
"ATMega8535",
"ATMega88",
"ATTiny861",
"ATMega16",
"ATMega168",
"ATMega328",
"ATMega32"
};
//-----------------------------------------------------------------------------
/** Constructor

To build the object, the serial connection to the device is synchronized, the
device is interrogated for details, and inforamtion about lock and fuse bits is
retrieved.

@param[in] p Serial Port object pointer
@param[in] parent Parent widget.
*/

AvrSerialProg::AvrSerialProg(SerialPort* p, uint initialBaudrate,bool commandLine,
                              bool debug,QWidget* parent): QDialog(parent)
{
    port = p;
    commandLineOnly = commandLine;
    debugMode = debug;
    if (debugMode) qDebug() << "Debug Mode";
    verify = true;
    upload = true;
    passThrough = true;
// Query the programmer and get device and programmer parameters
    bool queryOK = initializeProgrammer(initialBaudrate);
    readBlockMode = blockSupport;
    writeBlockMode = blockSupport;
    autoincrementMode = autoincrement;
// Set up the GUI if we are not using the command line
    if (! commandLineOnly)
    {
// Build the User Interface display from the Ui class in ui_mainwindowform.h
        bootloaderFormUi.setupUi(this);
// Don't allow chip erase by default
        bootloaderFormUi.chipEraseCheckBox->setChecked(false);
        bootloaderFormUi.chipEraseButton->setEnabled(false);
        bootloaderFormUi.chipEraseButton->setVisible(false);
        if (debugMode) bootloaderFormUi.debugModeCheckBox->setChecked(true);
	else bootloaderFormUi.debugModeCheckBox->setChecked(false);
        bootloaderFormUi.uploadProgressBar->setVisible(false);
        bootloaderFormUi.errorMessage->setVisible(false);
        bootloaderFormUi.passThroughEnable->setChecked(true);
// Set this as a default to verify any transfers
        bootloaderFormUi.verifyCheckBox->setChecked(true);
// Action if everything worked
        if (queryOK)
        {
          bootloaderFormUi.idDisplay->setText(identifier);
          bootloaderFormUi.signatureDisplay->setText(QString("0x%1%2%3")
                       .arg((uchar)signatureArray[2],2,16,QLatin1Char('0'))
                       .arg((uchar)signatureArray[1],2,16,QLatin1Char('0'))
                       .arg((uchar)signatureArray[0],2,16,QLatin1Char('0')));
          bootloaderFormUi.typeDisplay->setText(deviceType);
          if (lockFuse & 0x01)
              bootloaderFormUi.lockDisplay->setText(QString("0x%1")
                        .arg((uchar)lockBits,2,16,QLatin1Char('0')));
          else
          {
              bootloaderFormUi.lockDisplay->setText("");
              bootloaderFormUi.lockDisplay->setEnabled(false);
          }
          if (lockFuse & 0x0E)
          {
              QString fuseValues = "";
              if (lockFuse & 0x02)
                  fuseValues += "(L) " + QString("0x%1").arg(fuseBits,2,16,QLatin1Char('0'));
              if (lockFuse & 0x04)
                  fuseValues += " (H) " + QString("0x%1").arg(highFuseBits,2,16,QLatin1Char('0'));
              if (lockFuse & 0x08)
                  fuseValues += " (E) " + QString("0x%1").arg(extFuseBits,2,16,QLatin1Char('0'));
              bootloaderFormUi.fuseDisplay->setText(fuseValues);
          }
          else
              bootloaderFormUi.fuseDisplay->setEnabled(false);
          bootloaderFormUi.autoAddressCheckBox->setEnabled(false);
          bootloaderFormUi.autoAddressCheckBox->setChecked(autoincrement);
          bootloaderFormUi.writeBlockModeCheckBox->setEnabled(blockSupport);
          bootloaderFormUi.writeBlockModeCheckBox->setChecked(blockSupport);
          bootloaderFormUi.readBlockModeCheckBox->setEnabled(blockSupport);
          bootloaderFormUi.readBlockModeCheckBox->setChecked(blockSupport);
        }
// Action if something didn't work
        else
        {
            bootloaderFormUi.writeBlockModeCheckBox->setEnabled(false);
            bootloaderFormUi.readBlockModeCheckBox->setEnabled(false);
            bootloaderFormUi.autoAddressCheckBox->setEnabled(false);
            bootloaderFormUi.debugModeCheckBox->setEnabled(false);
            bootloaderFormUi.verifyCheckBox->setEnabled(false);
            bootloaderFormUi.writeCheckBox->setEnabled(false);
            bootloaderFormUi.openFileButton->setEnabled(false);
            bootloaderFormUi.lockFuseButton->setEnabled(false);
            bootloaderFormUi.OKButton->setEnabled(false);
            bootloaderFormUi.chipEraseCheckBox->setChecked(false);
            bootloaderFormUi.chipEraseButton->setEnabled(false);
            bootloaderFormUi.chipEraseButton->setVisible(true);
            bootloaderFormUi.errorMessage->setVisible(true);
            bootloaderFormUi.errorMessage->setText(errorMessage);
        }
    }
}

AvrSerialProg::~AvrSerialProg()
{
    port->close();
}

//-----------------------------------------------------------------------------
/** @brief Successful synchronization

@returns TRUE if the device responded eventually with a verified bootloader
response, and the attempt to get it into programming mode worked.
*/
bool AvrSerialProg::success()
{
    return synchronized;
}
//-----------------------------------------------------------------------------
/** @brief Error Message

@returns a message when the device didn't respond properly.
*/
QString AvrSerialProg::error()
{
    return errorMessage;
}
//-----------------------------------------------------------------------------

/** @defgroup This section comprises all the GUI action slots.

@{*/
//-----------------------------------------------------------------------------
/** @brief Close when cancel is activated.

No further action is taken, and the device may remain in the bootloader.
*/

void AvrSerialProg::on_cancelButton_clicked()
{
    reject();
}

//-----------------------------------------------------------------------------
/** @brief Change debug mode when checkbox changed.

*/

void AvrSerialProg::on_debugModeCheckBox_stateChanged()
{
    debugMode = bootloaderFormUi.debugModeCheckBox->isChecked();
}

//-----------------------------------------------------------------------------
/** @brief Change chip erase button when checkbox changed.

*/

void AvrSerialProg::on_chipEraseCheckBox_stateChanged()
{
    if (bootloaderFormUi.chipEraseCheckBox->isChecked())
    {
        bootloaderFormUi.chipEraseButton->setEnabled(true);
        bootloaderFormUi.chipEraseButton->setVisible(true);
    }
    else
    {
        bootloaderFormUi.chipEraseButton->setEnabled(false);
        bootloaderFormUi.chipEraseButton->setVisible(false);
    }
}

//-----------------------------------------------------------------------------
/** @brief Erase the chip when the chip erase button is clicked

*/

void AvrSerialProg::on_chipEraseButton_clicked()
{
    sendCommand('e');                   // "e" wipes the chip
    bootloaderFormUi.chipEraseButton->setEnabled(false);
    bootloaderFormUi.chipEraseButton->setVisible(false);
    bootloaderFormUi.chipEraseCheckBox->setChecked(false);
}

//-----------------------------------------------------------------------------
/** @brief Close when OK is activated.

When the programming is complete, the programmer is exited with the command 'E'
if the passthrough checkbox is enabled, and the target should start execution
with serial communications pass through.
*/

void AvrSerialProg::on_OKButton_clicked()
{
    if (bootloaderFormUi.passThroughEnable->isChecked())
        sendCommand('E');                   // "E" takes us out of programming mode
    accept();
}

//-----------------------------------------------------------------------------
/** @brief Open an AVR Intel Hex program file and perform requested operations.

Open file dialogue to select a file to upload and call a loader function.
A .hex file indicates an Intel Hex file for Flash memory
A .eep file indicates an intel hex file for EEPROM

@todo Add in EEPROM access, which involves the write and read page routines
issuing the different commands. This slot function will open the eep file.
*/

void AvrSerialProg::on_openFileButton_clicked()
{
    QString errorMessage;
    bool error = false;
//! Get filename of hex format to load
    QString filename = QFileDialog::getOpenFileName(this,
                                        "Intel Hex File to Upload",
                                        "./",
                                        "Intel Hex Files (*.hex)");
    if (! filename.isEmpty())
    {
        QFileInfo fileInfo(filename);
        QFile file(filename);
        if (file.open(QIODevice::ReadOnly))
        {
            error = loadHexGUI(&errorMessage, &file, 'F');
        }
    }
    if (error) QMessageBox::critical(this,"AVR Hex File Load Failure",errorMessage);
}
//-----------------------------------------------------------------------------
/** @brief Open a Target Device Specific dialogue for Lock and Fuse Bit Settings.

Open a dialogue to allow the lock and fuse bits to be displayed in a functional form.
The target device characteristics are queried to determine which dialogue to open.

@todo make this more elegant with an array of devices and features of importance.
*/

void AvrSerialProg::on_lockFuseButton_clicked()
{
//    bool sentOK = setProgrammingMode();
//    if (! sentOK) return;
    bool sentOK = getLockFuse(lockFuse,lockBits,fuseBits,highFuseBits,extFuseBits);
    if (! sentOK) return;
    if (partType == 328)
    {
        M328Dialog* m328DialogForm = new M328Dialog(port,this);
        m328DialogForm->setDefaults(lockBits,extFuseBits,highFuseBits,fuseBits);
        m328DialogForm->exec();
    }
    if (partType == 88)
    {
        M88Dialog* m88DialogForm = new M88Dialog(port,this);
        m88DialogForm->setDefaults(lockBits,extFuseBits,highFuseBits,fuseBits);
        m88DialogForm->exec();
    }
    else if (partType == 48)
    {
        M48Dialog* m48DialogForm = new M48Dialog(port,this);
        m48DialogForm->setDefaults(lockBits,extFuseBits,highFuseBits,fuseBits);
        m48DialogForm->exec();
    }
    else if (partType == 8535)
    {
        M8535Dialog* m8535DialogForm = new M8535Dialog(port,this);
        m8535DialogForm->setDefaults(lockBits,highFuseBits,fuseBits);
        m8535DialogForm->exec();
    }
    else if (partType == 16)
    {
        M16Dialog* m16DialogForm = new M16Dialog(port,this);
        m16DialogForm->setDefaults(lockBits,highFuseBits,fuseBits);
        m16DialogForm->exec();
    }
    else if (partType == 261)
    {
        T261Dialog* t261DialogForm = new T261Dialog(port,this);
        t261DialogForm->setDefaults(lockBits,extFuseBits,highFuseBits,fuseBits);
        t261DialogForm->exec();
    }
    else if (partType == 26)
    {
        T26Dialog* t26DialogForm = new T26Dialog(port,this);
        t26DialogForm->setDefaults(lockBits,highFuseBits,fuseBits);
        t26DialogForm->exec();
    }
    else if (partType == 2313)
    {
        T2313Dialog* t2313DialogForm = new T2313Dialog(port,this);
        t2313DialogForm->setDefaults(lockBits,extFuseBits,highFuseBits,fuseBits);
        t2313DialogForm->exec();
    }
    else if (partType == 12313)
    {
        S2313Dialog* s2313DialogForm = new S2313Dialog(port,this);
        s2313DialogForm->setDefaults();
        s2313DialogForm->exec();
    }
}
//-----------------------------------------------------------------------------
/** @brief Load a .hex file to either Flash or EEPROM with GUI feedback

This sets up the progress bar and calls the loadHex method to do the actual GUI
independent loading.

@param[out] errorMessage Error message to print if any failure occurs.
@param[in] file File already opened for loading.
@param[in] memType 'F' indicates flash memory, and 'E' indicates EEPROM. Passed to lower routines
@returns boolean indicating if an error occurred.
*/

bool AvrSerialProg::loadHexGUI(QString* errorMessage, QFile* file, const uchar memType)
{
    bootloaderFormUi.uploadProgressBar->setVisible(true);
    bootloaderFormUi.uploadProgressBar->setMinimum(0);
    bootloaderFormUi.uploadProgressBar->setMaximum(file->size());
    bootloaderFormUi.uploadProgressBar->setValue(0);
    bool upload = bootloaderFormUi.writeCheckBox->isChecked();
    bool verify = bootloaderFormUi.verifyCheckBox->isChecked();
    bool ok = loadHexCore(upload, verify, errorMessage, file, memType);
    bootloaderFormUi.uploadProgressBar->setValue(file->size());
    bootloaderFormUi.uploadProgressBar->setVisible(false);
    return ok;
}
//-----------------------------------------------------------------------------
/** @brief Update the progress bar.

*/

void AvrSerialProg::updateProgress(int progress)
{
    if (! commandLineOnly)
    {
        bootloaderFormUi.uploadProgressBar->setValue(progress);
        qApp->processEvents();
    }
    else if (! debugMode) std::cerr << "=";
}

//-----------------------------------------------------------------------------
/** @brief Get the read block mode setting.

*/

bool AvrSerialProg::getReadBlockMode()
{
    if (! commandLineOnly)
        return bootloaderFormUi.readBlockModeCheckBox->isChecked();
    else return readBlockMode;
}
//-----------------------------------------------------------------------------
/** @brief Get the write block mode setting.

*/

bool AvrSerialProg::getWriteBlockMode()
{
    if (! commandLineOnly)
        return bootloaderFormUi.writeBlockModeCheckBox->isChecked();
    else return writeBlockMode;
}
/**@}*/
//-----------------------------------------------------------------------------

/** @defgroup This section comprises Command Line Only methods.

@{*/
//-----------------------------------------------------------------------------
/** @brief Print out all device and programmer details.

This is intended for the non-GUI command line operation only.

*/

void AvrSerialProg::printDetails(void)
{
    qDebug() << "========= Detected Details ============";
    qDebug() << "Programmer " << identifier;
    qDebug() << QString("Lock Byte %1").arg(lockBits,2,16);
    qDebug() << QString("Fuse Byte %1").arg(fuseBits,2,16);
    qDebug() << QString("High Fuse Byte %1").arg(highFuseBits,2,16);
    qDebug() << QString("Extended Fuse Byte %1").arg(extFuseBits,2,16);
    qDebug() << QString("Signature %1 %2 %3")
                       .arg((uchar)signatureArray[2],2,16,QLatin1Char('0'))
                       .arg((uchar)signatureArray[1],2,16,QLatin1Char('0'))
                       .arg((uchar)signatureArray[0],2,16,QLatin1Char('0'));
    qDebug() << "Device Detected " << deviceType;
}
//-----------------------------------------------------------------------------
/** @brief Open an AVR Intel Hex program file and perform requested operations.

Open file to upload and call a loader function. This is for the command line
operation only.

A .hex file indicates an Intel Hex file for Flash memory
A .eep file indicates an intel hex file for EEPROM

@todo Add in EEPROM access, which involves the write and read page routines
issuing the different commands.
*/

bool AvrSerialProg::uploadHex(QString filename)
{
    QString errorMessage;
    bool error = false;
    qDebug() << "Uploading file " << filename;
    if (! filename.isEmpty())
    {
        QFileInfo fileInfo(filename);
        QFile file(filename);
        uint numberProgressSteps = (file.size())/44/(pageSize>>4);
        std::cerr << "|";
        for (uint n=0; n<numberProgressSteps;n++) std::cerr << "-";
        std::cerr << "|" << std::endl;
        if (file.open(QIODevice::ReadOnly))
        {
            std::cerr << " ";
            error = loadHexCore(upload, verify, &errorMessage, &file, 'F');
            std::cerr << std::endl;
        }
        else errorMessage = "File open error";
    }
    else errorMessage = "Filename is blank";
    if (error) qDebug() << errorMessage;
    return error;
}
//-----------------------------------------------------------------------------
/** @brief Open an AVR Intel Hex program file and perform requested operations.

Open file to upload and call a loader function. This is for the command line
operation only.

A .hex file indicates an Intel Hex file for Flash memory
A .eep file indicates an intel hex file for EEPROM

@todo Add in EEPROM access, which involves the write and read page routines
issuing the different commands.
*/

void AvrSerialProg::quitProgrammer()
{
    if (passThrough) sendCommand('E');                   // "E" takes us out of programming mode
}
/**@}*/
//-----------------------------------------------------------------------------

/** @defgroup This section comprises all the GUI independent methods.

@{*/
//-----------------------------------------------------------------------------
/** @brief Set control parameters.

This sets a boolean control parameter to a boolean value.

@param[in] parameter to be set, enum param type indicating the parameter.
*/

void AvrSerialProg::setParameter(param parameter, bool value)
{
    switch (parameter)
    {
    case VERIFY: verify = value;break;
    case UPLOAD: upload = value;break;
    case COMMANDLINEONLY: commandLineOnly = value;break;
    case DEBUG: debugMode = value;break;
    case READBLOCKMODE: readBlockMode = value;break;
    case WRITEBLOCKMODE: writeBlockMode = value;break;
    case AUTOINCREMENTMODE: autoincrementMode = value;break;
    case PASSTHROUGH: passThrough = value;
    }
}
//-----------------------------------------------------------------------------
/** @brief Load a .hex file to either Flash or EEPROM

Block loads cause difficulties, as a block relates to a page of flash memory
that is buffered on chip and then written. Therefore ensure that when a
block is sent, it starts and ends on a single page boundary. This requires
some gymnastics in regard to watching the address counter, identifying gaps, and
ensuring that they are taken into account.

Note also that the 16 bit words are stored MSB first in the buffer and in the target.

@param[in] upload Boolean indicating if an upload is to be done.
@param[in] verify Boolean indicating if a verification is to be done (exclusively or after upload).
@param[out] errorMessage Error message to print if any failure occurs.
@param[in] file File already opened for loading.
@param[in] memType 'F' indicates flash memory, and 'E' indicates EEPROM. Passed to lower routines
@returns boolean indicating if an error occurred.
*/

bool AvrSerialProg::loadHexCore(bool upload, bool verify, QString* errorMessage, QFile* file, const uchar memType)
{
    char inBuffer[256];                     // Buffer for serial read
    bool sentOK = true;
    bool verifyOK = true;
    int progress=0;

    if (upload || verify)
    {
        if (upload)
        {
/** If a program operation is requested, erase the application memory and open
the file stream (this will erase lock bits if not accessing a bootloader).*/
            if (debugMode) qDebug() << "Start Chip Erase";
            port->putChar('e');                 // erase all application memory
            int numBytes = 0;
            while (numBytes == 0)               // Give it more time - it may be long
                numBytes = checkCommand(1);     // We could get stuck here!
            *errorMessage = "Erase Fail";
            sentOK = readPort(inBuffer,numBytes);
            if (debugMode) qDebug() << "Finish Chip Erase";
        }
//! If erased OK then open file and start loading up the block buffer
      	if (sentOK)
      	{
      	    if (debugMode) qDebug() << "Start of Program Load";
            QTextStream stream(file);
            bool ok;
            uint runningAddress=0;      	// AVR Flash next address
            uint blockStartAddress=0;      	// AVR Flash start of block address
//! Create a buffer for a block write (greater than pagesize)
            uchar blockBuffer[256];       	// Holding for a block write
            uint blockIndex = 0;          	// track the number in the buffer
//! On the first pass the running address needs to be determined.
            bool firstPass = true;
            while (! stream.atEnd() && sentOK && verifyOK)
            {
              	QString line = stream.readLine();
/** Interpret the Intel Hex format line to get line length, record type and address.*/
               	uint lineLength = line.mid(1,2).toUInt(&ok,16);
               	uint address = (line.mid(3,4).toUInt(&ok,16));
               	uint recordType = line.mid(7,2).toUInt(&ok,16);
                progress += line.length();
				if (debugMode)
				{
	                qDebug() << line;
    	            qDebug() << "Line Length " << lineLength << "Start Address" << address << "Record" << recordType;
				}
                uint lineIndex = 0;	        // Index into line to be put in buffer
/** On the first pass, the address of the first byte must be determined. This is
only needed once for a block as the running address counter follows the address
to be programmed. If the data does not start on a page boundary then the running
address starts earlier and the buffer will be filled with dummy data.*/
                if (firstPass)
                {
                    firstPass = false;
                    runningAddress = address - address % pageSize;
    		            blockStartAddress = runningAddress;
                    *errorMessage = "Address Setting Failure";
                    sentOK = sendAddress(runningAddress);
				if (debugMode)
				{
                    qDebug() << "First Pass. runningAddress " << runningAddress << sentOK;
				}
                    if (! sentOK) break;
                }
                if (recordType != 0) lineLength = 1;    // fudge to get loop to go once at end
	        	    while (lineIndex < lineLength)
	        	    {
/** If a terminating record (record type = 0x01) occurs, then do not process any
more record data, but skip straight to the block upload of the remainder.*/
              	    if (recordType == 0)      	// Normal record
              	    {
/** If the next address to be programmed is earlier than that in the file,
then there is a gap in the byte stream. Fill the buffer with dummy data,
otherwise fill the local block buffer with the program data.*/
                       	if (runningAddress > address)
		          	        {
                            *errorMessage = "Address Tracking Error";
			                      sentOK = false;	    // error condition so get out
			                      break;
		          	        }
		          	        else if (runningAddress < address)
                                  blockBuffer[blockIndex] = 0xFF;
		          	        else
		          	        {
                            blockBuffer[blockIndex] =
                            	line.mid((lineIndex<<1)+9,2).toUInt(&ok,16);
			                      address++;	        // Follow runningAddress
    		                    lineIndex++;
      		    	        }
						if (debugMode)
						{
	                        qDebug() << address << lineIndex << runningAddress << blockIndex << line.mid(((lineIndex-1)<<1)+9,2) << blockBuffer[blockIndex];
						}
                        ++runningAddress;       // track the address
                        ++blockIndex;
                    }
    		            else
  		    	        lineIndex = lineLength; // Terminate the loop
/** When the block buffer has been filled, start a blockload command that transfers
the block up to the device memory.*/
                    if (((blockIndex >= pageSize) || (recordType != 0)) && (blockIndex > 0))
                    {
                        if (debugMode)
                        {
                            qDebug() << "Write/Verify Page at address"
                                 << QString("%1 ").arg(blockStartAddress,2,16,QLatin1Char('0'))
                                 << " length " << blockIndex;
//                                hexDumpBuffer(blockBuffer,blockIndex,blockStartAddress);
                        }
/** We will attempt to write and verify the page. If it doesn't write we drop out, but if it doesn't verify we will
continue retrying five times. Once written OK, bump the start address to the next page and reset the buffer. */
                        verifyOK = false;
                        sentOK = true;
                        uint retryCount = 5;
                        *errorMessage = "Write Page Failure";
                        while (sentOK && (! verifyOK) && (retryCount > 0))
                        {
                            if (upload)
                            {
                                sentOK = writePage(blockBuffer,blockIndex,blockStartAddress,memType);
                                verifyOK = true;
                            }
                            if (sentOK && verify)
                                verifyOK = verifyPage(blockBuffer,blockIndex,blockStartAddress,memType);
                            retryCount--;
                        }
      			            blockStartAddress += pageSize;
                        blockIndex = 0;         // reset the block index
                        updateProgress(progress);
                    }
    	    	    }
            }
      	    if (debugMode) qDebug() << "End of Program Load/Verify";
  	    }
        if (! sentOK)
            *errorMessage = QString("File did not load properly, retry\n")+*errorMessage;
        else if (! verifyOK)
            *errorMessage = QString("File did not verify\nMay be temporary, retry");
    }
    return !(sentOK && verifyOK);
}

//-----------------------------------------------------------------------------
/** @brief Initialize by querying for programmer and device parameters.

This performs all the hardware level initialization, separate from the GUI
aspects, testing each phase in turn and aborting if there is an error
in any of the phases.

@returns boolean indicating if an error occurred.
*/

bool AvrSerialProg::initializeProgrammer(uint initialBaudrate)
{
    synchronized = false;
    bool sentOK = false;        // Tracks communication integrity
/** Setup port for transmission */
    sentOK = port->initPort(initialBaudrate,100);
    if (debugMode) qDebug() << "Initialized";
    if (! sentOK)
    {
        errorMessage = QString("Unable to initialize the serial port.\n"
                               "Check connections to the programmer.\n"
                               "You may (but shouldn't) need root privileges.");
        return false;
    }
/** Attempt to synchronize the baudrate and establish the bootloader presence.*/
    sentOK = syncProgrammer(port,initialBaudrate);
    if (! sentOK)
    {
        errorMessage = QString("Unable to synchronize the device");
        return false;
    }
/** Proceed to verify the bootloader and pull in some information about its
capabilities. In the GUI the autoAddress and block mode capabilities are
used to set checkboxes that can be modified by the user before uploading a
file. This allows blockmode to be turned off if desired. */
    sentOK = leaveProgrammingMode();
    if (! sentOK)
    {
        errorMessage = "Unable to leave Programming Mode";
        return false;
    }
    synchronized = true;
    sentOK = getVersion(identifier);
    if (! sentOK)
    {
        errorMessage = "Unable to get Programmer Identifier";
        return false;
    }
// Put programmer into programming mode
    sentOK = setProgrammingMode();
    if (! sentOK)
    {
        errorMessage = "Programming Mode Failed";
        return false;
    }
// Issue a signature read request
    sentOK = getSignature(signatureArray);
    if (! sentOK)
    {
        errorMessage = "Unable to get Signature Bytes";
        return false;
    }
// Use the signature to search for the device type, if it is in the part table
    deviceType = "Unknown";
    bool found=false;                   // Indicates if the target device is supported
    uint partNo = 0;                    // Search the table for our device.
    if (signatureArray[2] == 0x1E)
    {
        while ((partNo < NUMPARTS) && (! found))
        {
            found = ((part[partNo][0] == (uchar)signatureArray[1])
                     && (part[partNo][1] == (uchar)signatureArray[0]));
            partNo++;
        }
    }
    lockFuse = 0;
    partType = 0;
    if (found)
    {
        deviceType = partName[--partNo];
        lockFuse = part[partNo][5];
        partType = part[partNo][2];
    }
// Get the lock and fuse bits and display
    sentOK = getLockFuse(lockFuse,lockBits,fuseBits,highFuseBits,extFuseBits);
    if (! sentOK)
    {
        errorMessage = "Unable to get Fuse/Lock Bytes";
        return false;
    }
// Issue an autoaddress confirm request
    sentOK = getAutoAddress(autoincrement);
    if (! sentOK)
    {
        errorMessage = "Unable to get Autoincrement Capability";
        return false;
    }
// Issue a block transfer support confirm request
    sentOK = getBlockSupport(blockSupport,pageSize);
    if (! sentOK)
    {
        errorMessage = "Unable to get Block Support Capability";
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
/** @brief Debug: Dump a buffer in Hex to the screen.

@param[in] blockBuffer: Read only pointer to the buffer containing the data to dump.
@param[in] blockLength: Length of block to be dumped.
@param[in] address: Starting address for which display is relevant.
*/

void AvrSerialProg::hexDumpBuffer(const uchar* blockBuffer,
                              	  const uint blockLength,
                                  const uint address)
{
    QString line = QString("%1: ").arg(address,2,16,QLatin1Char('0'));
    uint lineCount = 0;
    uint runningAddress = address;
    for (uint blockIndex = 0; blockIndex < blockLength; blockIndex++)
    {
        line += QString("%1 ").arg((uchar)blockBuffer[blockIndex],2,16,QLatin1Char('0'));
        runningAddress++;
        if (lineCount++ >= 15)
        {
            qDebug() << line;
            line = QString("%1: ").arg(runningAddress,2,16,QLatin1Char('0'));
            lineCount = 0;
        }
    }
    if (lineCount > 0) qDebug() << line;
}
//-----------------------------------------------------------------------------
/** @brief Verify a single page in the bootloader

If a verification mismatch occurs, a console message is output with the
details. This may not be a true mismatch, but a result of a bad read operation.

@param[in] blockBuffer Read only pointer to the buffer containing the data to compare.
@param[in] blockLength Length of block to compare.
@param[in] address Address to start verifying.
@param[in] memType 'F' indicates flash memory, and 'E' indicates EEPROM
@returns true if the verification was successful.
*/

bool AvrSerialProg::verifyPage(const uchar* blockBuffer,
                               const uint blockLength,
                               const uint address, const uchar memType)
{
    if (debugMode)
    {
        qDebug() << "Verify Page";
        qDebug() << "File Contents Read"
             << QString("Address 0x%1").arg((uchar)address,2,16,QLatin1Char('0'))
             << QString("BlockLength 0x%1").arg((uchar)blockLength,2,16,QLatin1Char('0'));
        hexDumpBuffer(blockBuffer,blockLength,address);
    }
    uchar inBuffer[256];                // Buffer for serial read
//! Download a page from the device memory
    bool verifyOK = readPage(inBuffer,blockLength,address,memType);
    if (debugMode) qDebug() << "Read Device OK";
//! Compare byte by byte
    if (verifyOK)
    {
        uint index = 0;
        while (index < blockLength)
        {
            if (inBuffer[index]!=blockBuffer[index])
            {
                verifyOK = false;
                qDebug() << "Mismatch at " << QString("%1").arg(address+index,2,16,QLatin1Char('0'))
	                     << "Device Value "
                         << QString("0x%1")
                                   .arg((uchar)inBuffer[index],2,16,QLatin1Char('0'))
                         << "Comparison Value "
                         << QString("0x%1")
                                   .arg((uchar)blockBuffer[index],2,16,QLatin1Char('0'));
                break;
            }
            index++;
        }
    }
    if (debugMode)
    {
        if (verifyOK) qDebug() << "Verified OK";
        else qDebug() << "Verification Failure";
    }
    return verifyOK;
}
/**@}*/
/****************************************************************************/
/** @defgroup accessDevice Functions to access the device

These functions pull together all code that accesses the device through the
serial communications port. They implement the various programmer commands
needed to read device characteristics and to activate programmer actions.
@{*/
//-----------------------------------------------------------------------------
/** @brief Try to establish sync with the bootloader.
 
This function attempts to determine the baudrate used by the target bootloader
by cycling the baudrate through a set of standard values and looking for a valid
response to certain commands. No attempt is made to try different sets of serial
formats. A common setting is assumed. We issue IDLE characters until a valid "?"
character response is obtained. The IDLE character is defined by the acquisition
application, but in general could be just any character. We need to timeout any
transmission in case there is no response.

Because of the possible existence of the Acquisition application also check to
see if there is an IDLE response. If so, then give it a "jump to bootloader"
command. In other applications this would probably have no effect but it is well
to be aware of this code presence.

A spurious "?" may occur, so check with a simple bootloader command. The
USB-serial adaptors seem to send this when an error condition is received,
which is likely to occur when the baudrates don't match. The number of attempts
is limited.

@param[in] port Serial port object pointer.
@param[in] initBaudrate The baud rate to begin the search.
@returns true if the synchronization was successful.
*/

bool AvrSerialProg::syncProgrammer(SerialPort* port,
                                   const uchar initBaudrate)
{
    bool ok;
    uchar baudrate = initBaudrate;
    char inBuffer [256];            // Read buffer to check on IDLE responses
    uint timeout;                   // Setup a timer to deal with non-response
    uchar attempts = 14;            // Maximum number of attempts
    bool unsynched = true;
    bool first = true;              // Acquisition command not yet sent
    if (debugMode) qDebug() << "Attempt Programmer Synchronization";
    while (unsynched)
    {
/** The IDLE character would be recognised by the acquisition program, so if
an IDLE comes back we are probably in that program. However the bootloader if
present will respond with a "?", so we are probably there.*/
        ok = port->putChar(IDLE_CHAR); // Issue an IDLE character
        timeout = TIMEOUTCOUNT;
        int checkBytes = 0;    // Check if any response received
        while ((--timeout > 0) && (checkBytes <= 0))
            checkBytes = port->bytesAvailable();
        uint numBytes = checkBytes;
        if (checkBytes > 0)       // If we received something
        {
			if (debugMode) qDebug() << QString("Received %1 Bytes").arg(checkBytes,2,16);
            port->read(inBuffer,numBytes);
			if (debugMode) qDebug() << QString("Character %1").arg((uchar)inBuffer[0],2,16);
            if ((uchar)inBuffer[0] == IDLE_CHAR)
            {
/** A response means the acquisition application is running, therefore try to
send a "jump to bootloader" packet. Wait a bit to give it a chance to respond,
so don't send it again if another IDLE_CHAR arrives, as it is likely to be the
application still responding. Keep searching for the bootloader response.*/
                if (first)
                {
                    first = false;
                    qDebug() << "Found Possible Acquisition application";
                    port->putChar(IDLE_CHAR);
                    port->putChar(SYNC_CHAR);
                    port->putChar(0x00);
                    port->putChar(0x01);
                    port->putChar(0x40);
                    port->putChar(0x41);
                    port->putChar(EOM_CHAR);
                    baudrate = initBaudrate;
                }
            }
            else if ((QChar)inBuffer[0] == '?') // Possible bootloader found
                unsynched = false;
        }
        else qDebug() << "Timeout";

        if (unsynched)              // If timeout or bad character
        {
            port->close();          // Close port for reinitialization
            port->initPort(++baudrate,100);  // Retry with new baudrate
            if (--attempts == 0)    // limit attempts to two cycles through
                return false;
        }
/** If an apparent valid bootloader response arrives, need to check that the
bootloader is present. Use a simple command with known reply.*/
        else
        {
            port->putChar('a');     // Issue a autoaddress confirm request
            timeout = 10000;
            int checkBytes = 0;    // Check if any response received
            while ((--timeout > 0) && (checkBytes <= 0))
                checkBytes = port->bytesAvailable();
            uint numBytes = checkBytes;
            if (checkBytes > 0)       // If we received something
            {
				if (debugMode) qDebug() << QString("Bootloader test: Received %1 Bytes").arg(checkBytes,2,16);
                port->read(inBuffer,numBytes);
				if (debugMode) qDebug() << QString("Character %1").arg((uchar)inBuffer[0],2,16);
                if ((uchar)inBuffer[0] == 'Y') break;
            }
            unsynched = true;
            qDebug() << "Not a bootloader response";
        }
    }
    port->close();                  // Close port for reinitialization
    if (debugMode) qDebug() << "Baudrate index found " << baudrate;
    port->initPort(baudrate,100);    // Reopen with minimal timeout
    return true;
}
//-----------------------------------------------------------------------------
/** @brief Resynchronize the Programmer.

Try some tricks to resynchronise the Programmer in case it is in the middle of
a command when it got lost. Send a string of ESCs as these do not get a response
from the Programmer but can be used to wriggle out of the middle of a command.
Then send an 'a' command which should have one response, either 'Y' or 'N'.

@returns true if a valid command was received.
*/

bool AvrSerialProg::resyncProgrammer()
{
    char inBuffer[256];                 // Buffer for serial read
    for (uchar n=0; n<64;++n)
        port->putChar(0x1B);            // Spew out ESC characters
    port->putChar('a');                 // Check with any command
    if (debugMode) qDebug() << "Sent <a>";
    int numBytes = checkCommand(1);
    return readPort(inBuffer,numBytes);
}
//-----------------------------------------------------------------------------
/** @brief Put device into programming mode.

@returns true if the action was successful.
*/
bool AvrSerialProg::setProgrammingMode()
{
    char inBuffer[32];
    port->putChar('P');                         // Leave programming mode
    if (debugMode) qDebug() << "Sent <P>";
    int numBytes = checkCommand(1);
    bool sentOK = (numBytes > 0);
    if(sentOK)
    {
        sentOK = readPort(inBuffer,numBytes);    // Pull in response (0x0D)
        if (inBuffer[0] == '?') sentOK = false;
    }
    return sentOK;
}

//-----------------------------------------------------------------------------
/** @brief Put device out of programming mode.

@returns true if the action was successful.
*/
bool AvrSerialProg::leaveProgrammingMode()
{
    char inBuffer[32];
    port->putChar('L');                         // Leave programming mode
    int numBytes = checkCommand(1);
    bool sentOK = (numBytes > 0);
    if(sentOK)
    {
        sentOK = readPort(inBuffer,numBytes);    // Pull in response (0x0D)
    }
    return sentOK;
}

//-----------------------------------------------------------------------------
/** @brief Get the three signature bytes in hexadecimal form.

@param[in] signature Array of three signature bytes
@returns true if the action was successful.
*/
bool AvrSerialProg::getSignature(char* signature)
{
    port->putChar('s');
    if (debugMode) qDebug() << "Sent <s>";
    int numBytes = checkCommand(3);
    bool sentOK = (numBytes == 3);
    if(sentOK)
    {
        sentOK = readPort(signature,numBytes);
    }
	if (debugMode) qDebug() << QString("Signature 0x%1%2%3")
                       .arg((uchar)signatureArray[2],2,16,QLatin1Char('0'))
                       .arg((uchar)signatureArray[1],2,16,QLatin1Char('0'))
                       .arg((uchar)signatureArray[0],2,16,QLatin1Char('0'));
    return sentOK;
}

//-----------------------------------------------------------------------------
/** @brief Get the fuse and lock bit settings in binary (uchar) form.

@param[in]  lockFuse A byte holding read and write capability for the various lock/fuse bytes
@param[out] lockBits The byte of lock bits
@param[out] fuseBits The byte of fuse bits
@param[out] highFuseBits The byte of high fuse bits
@param[out] extFuseBits The byte of extended fuse bits
@returns true if the action was successful.
*/
bool AvrSerialProg::getLockFuse(const uchar lockFuse, uchar& lockBits, uchar& fuseBits,
                                uchar& highFuseBits, uchar& extFuseBits)
{
    if ((lockFuse & 0x08) == 0) return true;        // Early models cannot read anything
    char inBuffer[32];
    bool sentOK = false;
    lockBits = 0;
    fuseBits = 0;
    highFuseBits = 0;
    extFuseBits = 0;
    if (lockFuse & 0x01)
    {
        port->putChar('r');         // Issue a lock byte read  request
	    if (debugMode) qDebug() << "Sent <r>";
        int numBytes = checkCommand(1);
        sentOK = (numBytes > 0);
        if(sentOK)
        {
            sentOK = readPort(inBuffer,numBytes);
            lockBits = inBuffer[0];
        }
    }
    if (lockFuse & 0x02)
    {
        port->putChar('F');         // Issue a low Fuse byte read request
	    if (debugMode) qDebug() << "Sent <F>";
        int numBytes = checkCommand(1);
        sentOK = (numBytes > 0);
        if(sentOK)
        {
            sentOK = readPort(inBuffer,numBytes);
            fuseBits = inBuffer[0];
        }
    }
    if (lockFuse & 0x04)
    {
        port->putChar('N');         // Issue a high Fuse byte read request
	    if (debugMode) qDebug() << "Sent <N>";
        int numBytes = checkCommand(1);
        sentOK = (numBytes > 0);
        if(sentOK)
        {
            sentOK = readPort(inBuffer,numBytes);
            highFuseBits = inBuffer[0];
        }
    }
    if (lockFuse & 0x08)
    {
        port->putChar('Q');         // Issue a extended Fuse byte read request
 	    if (debugMode) qDebug() << "Sent <Q>";
        int numBytes = checkCommand(1);
        sentOK = (numBytes > 0);
        if(sentOK)
        {
            sentOK = readPort(inBuffer,numBytes);
            extFuseBits = inBuffer[0];
        }
    }
    return sentOK;
}

//-----------------------------------------------------------------------------
/** @brief Get the autoaddressing capability in boolean form.

@param[out] autoincrement A boolean indicating if autoincrementing is supported.
@returns true if the action was successful.
*/
bool AvrSerialProg::getAutoAddress(bool& autoincrement)
{
    char inBuffer[32];
    port->putChar('a');
    if (debugMode) qDebug() << "Sent <a>";
    int numBytes = checkCommand(1);
    bool sentOK = (numBytes > 0);
    if(sentOK) sentOK = readPort(inBuffer,numBytes);
    autoincrement = ((QString) inBuffer[0] == "Y");
    return sentOK;
}

//-----------------------------------------------------------------------------
/** @brief Get the block transfer capability in boolean form.

@param[out] blockSupport A boolean indicating if block transfer is supported.
@param[out] pageSize An unsigned int of the pagesize of the device FLASH.
@returns true if the action was successful.
*/
bool AvrSerialProg::getBlockSupport(bool& blockSupport, uint& pageSize)
{
    char inBuffer[32];
    port->putChar('b');
    if (debugMode) qDebug() << "Sent <b>";
    int numBytes = checkCommand(3);
    bool sentOK = (numBytes > 0);
    if(sentOK) sentOK = readPort(inBuffer,numBytes);
    pageSize = ((uint) inBuffer[2] + ( (uint) inBuffer[1] << 8)) % 256;
    blockSupport = ((inBuffer[0] == 'Y') && (pageSize > 0));
    if (! blockSupport) pageSize = 1;
    return sentOK;
}

//-----------------------------------------------------------------------------
/** @brief Get the Version number and Programmer Identifier in QString form.

@param[out] identifier The QString containing 7 Programmer identifier characters,
a "version" string and 2 version characters.
@returns true if the action was successful.
*/
bool AvrSerialProg::getVersion(QString& identifier)
{
    char inBuffer[32];
    port->putChar('S');       // Issue an ident command
    if (debugMode) qDebug() << "Sent <S>";
    int numBytes = checkCommand(7);
    bool sentOK = (numBytes > 0);
    if(sentOK) sentOK = readPort(inBuffer,numBytes);
    inBuffer[7] = 0;
    identifier = (QString) inBuffer;
    port->putChar('V');        // Issue a version command
    if (debugMode) qDebug() << "Sent <V>";
    numBytes = checkCommand(2);
    sentOK = (numBytes > 0);
    if(sentOK) sentOK = readPort(inBuffer,numBytes);
    inBuffer[2] = 0;
    identifier += " version " + (QString) inBuffer;
	if (debugMode) qDebug() << "Identifier " << identifier;
    return sentOK;
}
//-----------------------------------------------------------------------------
/** @brief Write a single page to the bootloader.

The buffer ends up with the two bytes stored as low byte first followed by high byte.
A 1ms delay is inserted after each character is written to prevent the programmer
from being swamped with serial data. This may need to be changed. At this point
our blocklength is at most that reported by the programmer as being the page size,
so there will be no need for additional delay while a page is being written. This
program will synchronize with the returned acknowledgement. If the blocklength
is later changed to be greater, then we need to break the transmissions down to
page lengths and insert a 9ms additional delay after each page is transmitted.

@param[in] blockBuffer Read only pointer to the buffer containing the data to send.
@param[in] blockLength Length of block to be sent.
@param[in] address Address to start programming.
@param[in] memType 'F' indicates flash memory, and 'E' indicates EEPROM
@returns true if the write was successful.
*/

bool AvrSerialProg::writePage(const uchar* blockBuffer,
                              const uint blockLength,
                              const uint address, const uchar memType)
{
    if (debugMode)
    {
        qDebug() << "Write Individual Page from file";
		if (debugMode)
		{
	        hexDumpBuffer(blockBuffer,blockLength,address);
		}
    }
    char inBuffer[256];                     // Buffer for serial read
    int numBytes;
//    usleep(3000);                           // Insert an additional delay for programmer
    bool writeOK = sendAddress(address);    // Starting address
    if (!writeOK) qDebug() << "Address Setting Failure";
//! The block mode checkbox can be used to control this behaviour.
    if (writeOK && getWriteBlockMode())
    {
        if (debugMode) qDebug() << "Transmit Block to Target Flash Memory"
                                << QString("%1").arg(blockLength,2,16,QLatin1Char('0')) << "Bytes";
        port->putChar('B');		            // Write block of data
        port->putChar((uchar) ((blockLength >> 8) & 0xFF)); // High Byte first
        port->putChar((uchar) (blockLength & 0xFF));        // Then Low Byte
        port->putChar(memType);		        // indicate flash memory
        for (uint index = 0;index < blockLength;index++)
        {
            port->putChar(blockBuffer[index]);
            usleep(1000);                   // Delay 1 ms to allow time for slow programmer to catch up
        }
	    if (debugMode) qDebug() << "Sent <B> plus block of data";
        numBytes = checkCommand(1);
        writeOK = readPort(inBuffer,numBytes);
        if (!writeOK) qDebug() << "Block Write Response Failure"
                               << blockLength << numBytes
                               << QString("%1").arg(inBuffer[0],2,16,QLatin1Char('0'));
    }
/** If block mode is not used, send each two-byte word first for the whole page
to put it into the AVR page buffer, then reset the address back to point to the
start of the page so that a page write command can be issued. The address then
needs to be set again to its position at the end of the page buffer.

This mode will be slower because it involves additional serial link transfers.

This section relies on the blocklength being less than or equal to the page size.
If it is larger, then the page will be overwritten.*/
    else                       		        // Don't use block loads
    {
        if (debugMode) qDebug() << "Transmit Wordwise to Target Flash buffer";
        uint index = 0;
        while (index < blockLength)
        {
            port->putChar('c');       	    // lower byte sent first
            uchar sendChar = blockBuffer[index++];
            port->putChar(sendChar);
		    if (debugMode) qDebug() << "Sent <c> plus low byte";
            numBytes = checkCommand(1);
            writeOK = readPort(inBuffer,numBytes);
            if (!writeOK) qDebug() << "Low Byte Write Response Failure at Address:"
                                   << QString("%1 ").arg(address,2,16,QLatin1Char('0'));
            if (! writeOK) break;
            port->putChar('C');       	    // upper byte sent second
            port->putChar(blockBuffer[index++]);
		    if (debugMode) qDebug() << "Sent <C> plus high byte";
            numBytes = checkCommand(1);
            writeOK = readPort(inBuffer,numBytes);
            if (!writeOK) qDebug() << "High Byte Write Response Failure at Address:"
                                   << QString("%1 ").arg(address,2,16,QLatin1Char('0'));
            if (! writeOK) break;
        }
        if (writeOK)                   	    // Page address
            writeOK = sendAddress(address);
        if (writeOK)
        {
            if (debugMode) qDebug() << "Commit Page to Flash";
            port->putChar('m');       	    // commit the page
		    if (debugMode) qDebug() << "Sent <m>";
            numBytes = checkCommand(1);
            writeOK = readPort(inBuffer,numBytes);
            if (!writeOK) qDebug() << "Page Write Response Failure at Address:"
                                   << QString("%1 ").arg(address,2,16,QLatin1Char('0'))
                                   << QString("%1").arg(inBuffer[0],2,16,QLatin1Char('0'));
        }
    }
    return writeOK;
}
//-----------------------------------------------------------------------------
/** @brief Read a single page from the bootloader

Read a page from the device memory into a buffer.

@param[in] blockBuffer Pointer to a buffer to contain the data.
@param[in] blockLength Length of block to read.
@param[in] address Address to start reading.
@param[in] memType 'F' indicates flash memory, and 'E' indicates EEPROM
@returns true if the read was successful.
*/

bool AvrSerialProg::readPage(uchar* blockBuffer,
                             const uint blockLength,
                             const uint address, const uchar memType)
{
    char inBuffer[256];                     // Buffer for serial read
    int numBytes = -1;
    bool readOK = sendAddress(address);
//! The block mode checkbox can be used to control this behaviour.
    if (readOK && getReadBlockMode())
    {
        if (debugMode) qDebug() << "Read Block from Target Flash Memory"
                                << QString("%1").arg(blockLength,2,16,QLatin1Char('0')) << "Bytes";
        port->putChar('g');	                // Read a block of Flash memory
        port->putChar((uchar) ((blockLength >> 8) & 0xFF));     //High Byte
        port->putChar((uchar) (blockLength & 0xFF));            // Low byte
        port->putChar(memType);   	        // indicate flash memory
	    if (debugMode) qDebug() << "Sent <g> plus address and byte";
        numBytes = checkCommand(blockLength);
        readOK = (numBytes > 0);
        if (! readOK)
            qDebug() << "Read Fail";
        else
	    {
            port->read(inBuffer,numBytes);
            if (debugMode) qDebug() << "Number of bytes read" << QString("%1").arg(numBytes,2,16,QLatin1Char('0'));
	        for (int bufferIndex = 0;bufferIndex < numBytes;bufferIndex++)
		        blockBuffer[bufferIndex] = inBuffer[bufferIndex];
	    }
    }
/** If block mode is not used, read each two-byte word for the whole page.
Word comes as low first then high.*/
    else                      	            // Don't use block loads
    {
        if (debugMode) qDebug() << "Read Page Wordwise from Target Flash Memory";
        uint bufferIndex = 0;
        while (bufferIndex < blockLength)
        {
            port->putChar('R');             // Read word
		    if (debugMode) qDebug() << "Sent <R>";
            readOK = (checkCommand(2) == 2);
            if (! readOK)
            {
                qDebug() << "Read Fail";
                break;
            }
            port->read(inBuffer,2);
            blockBuffer[bufferIndex++] = inBuffer[1];
            blockBuffer[bufferIndex++] = inBuffer[0];
        }
    }
    if (debugMode)
    {
        qDebug() << "Target Flash Memory Contents Read"
             << QString("Address 0x%1").arg((uchar)address,2,16,QLatin1Char('0'))
             << QString("BlockLength 0x%1").arg((uchar)blockLength,2,16,QLatin1Char('0')) << numBytes;
        hexDumpBuffer(blockBuffer,blockLength,address);
    }
    return readOK;
}
//-----------------------------------------------------------------------------
/** @brief Set the FLASH address in the bootloader.

The address is sent MSB first, then LSB

@param[in] address Address to be set.
@returns true if the command was valid.
*/

bool AvrSerialProg::sendAddress(const uint address)
{
    char inBuffer[256];                 // Buffer for serial read
    uint wordAddress = (address >> 1);  // word address
    bool sendOK = false;
    for (uint i = 0; i < 2; i++)        // Give it a couple of tries
    {
        port->putChar('A');             // address command
        port->putChar((uchar) ((wordAddress >> 8) & 0xFF));
        port->putChar((uchar) (wordAddress & 0xFF));
	    if (debugMode) qDebug() << "Sent <V> plus address";
        int numBytes = checkCommand(1);
        sendOK = readPort(inBuffer,numBytes);
		if (debugMode)
		{
	        qDebug() << "Send Address. numbytes: " << numBytes << sendOK << "Index " << i;
		}
        if (sendOK) break;
        else resyncProgrammer();
    }
    return sendOK;
}
//-----------------------------------------------------------------------------
/** @brief Read the port and check for a valid AVRPROG command.

This is intended to pull in a response to a command and basically just
checks that the command is valid (an invalid command will elicit a response
'?' from the bootloader). Do not use this while reading binary data as a '?' may
occur in the data stream. Use only for commands. A side effect is that an error
in the serial interface can result in a '?' character being returned. This has
been noticed with a serial to USB converter.

@param[out] inBuffer Pointer to the buffer that will receive the read data.
@param[in] numBytes Number of bytes to be read. This will block until all the
requested bytes have been read.
@returns false if numBytes is zero, or a '?' is the first character.
*/

bool AvrSerialProg::readPort(char* inBuffer, const int numBytes)
{
    if ((numBytes == 0)) return false;
    port->read(inBuffer,numBytes);
    return !(inBuffer[0] == '?');
}
//-----------------------------------------------------------------------------
/** @brief Check an AVRPROG command.

This does not read the port but checks for the presence of waiting characters.
When the number waiting equals the expected number, or a timeout occurs, the
function returns. This provides the subsequent read call with all the characters
needed to complete its task without complications.

At least one byte is always returned by an AVRPROG command except for the ESC
command. If the expected bytes is specified as zero, the function will
return for any non-zero number of bytes, otherwise it will return only when
the specified number of bytes is received.

The timeout is programmed to avoid program hangs if the serial interface is interrupted.
However there is no error condition returned so the program merrily continues on.
Make sure the timeout count is set to a high enough value to allow for slow links.
You will be sure of this when your avr program uploads without errors.

@param[in] expectedBytes: The number of bytes expected to be returned.
@returns Number of bytes actually received (0 if timeout).
*/

int AvrSerialProg::checkCommand(const int expectedBytes)
{
    uint timeout = 0;               // Setup a timer to deal with non-response
    int numBytes = 0;               // Check if any response received
    int numBytesPrevious = 0;
    bool match = false;
    while ((++timeout < TIMEOUTCOUNT) && (! match))
    {
        numBytes = port->bytesAvailable();
        if (expectedBytes > 0)
        {
            match = (numBytes == expectedBytes);
        }
        else
            match = (numBytes > 0);
        if (numBytes > numBytesPrevious)
            timeout = 0;            // Reset timeout as we are getting something
        numBytesPrevious = numBytes;
    }
    if (numBytes < 0) numBytes = 0;
    if (!match) qDebug() << "Check-Command Timeout" << numBytes << "Bytes Received" << expectedBytes << "Expected";
    return numBytes;
}
//-----------------------------------------------------------------------------
/** @brief Send a single character command to the programmer.

This is used to command the programmer without needing a response.

@param[in] command. A single character.
*/

void AvrSerialProg::sendCommand(const char command)
{
    char inBuffer[16];
    int numBytes;
    port->putChar(command);
    if (debugMode) qDebug() << "Sent " << command;
    numBytes = checkCommand(1);
    readPort(inBuffer,numBytes);
}
/**@}*/
//-----------------------------------------------------------------------------
