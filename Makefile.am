AUTOMAKE_OPTIONS = subdir-objects
AM_CPPFLAGS =-I$(top_srcdir)/inc
AM_CXXFLAGS = -std=c++11 -fno-rtti -fno-exceptions -pthread

noinst_PROGRAMS = emmcdl

emmcdl_LDADD = -lrt

emmcdl_SOURCES = \
               src/crc.cpp\
               src/dload.cpp\
               src/emmcdl.cpp\
               src/firehose.cpp\
               src/ffu.cpp\
               src/sahara.cpp\
               src/partition.cpp\
               src/protocol.cpp\
               src/usbport.cpp\
               src/usb_linux.c\
               src/sparse.cpp\
               src/xmlparser.cpp

if SYSDEPS_WIN32
AM_CPPFLAGS += -D_WIN32
emmcdl_SOURCES += \
               src/diskwriter_windows.cpp\
               src/sysdeps_win32.cpp
else
emmcdl_SOURCES += \
               src/diskwriter_linux.cpp
endif
