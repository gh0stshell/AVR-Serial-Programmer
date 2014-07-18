PROJECT =       AVR Serial Programmer
TEMPLATE =      app
TARGET          += 
DEPENDPATH      += .
include(../../qextserialport/src/qextserialport.pri)

OBJECTS_DIR     = obj
MOC_DIR         = moc
UI_HEADERS_DIR  = ui
UI_SOURCES_DIR  = ui
LANGUAGE        = C++
CONFIG          += qt warn_on release

# Input
FORMS           += avrserialprog.ui \
                   m328Dialog.ui  m88Dialog.ui   m48Dialog.ui    m8535Dialog.ui  m16Dialog.ui \
                   t261Dialog.ui  s2313Dialog.ui t26Dialog.ui
HEADERS         += avrserialprog.h serialport.h \
                   m328Dialog.h   m88Dialog.h    m48Dialog.h     m8535Dialog.h   m16Dialog.h \
                   t261Dialog.h   t26Dialog.h    t2313Dialog.h   s2313Dialog.h
SOURCES         += avrserialprogmain.cpp avrserialprog.cpp serialport.cpp \
                   m328Dialog.cpp m88Dialog.cpp  m48Dialog.cpp   m8535Dialog.cpp m16Dialog.cpp \
                   t261Dialog.cpp t26Dialog.cpp  t2313Dialog.cpp s2313Dialog.cpp

