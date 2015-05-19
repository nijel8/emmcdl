AUTOMAKE_OPTIONS = subdir-objects
INCLUDES=-I$(top_srcdir)/inc
AM_CXXFLAGS= -std=c++11

noinst_PROGRAMS = emmcdl

emmcdl_SOURCES = \
               src/crc.cpp\
               src/dload.cpp\
               src/emmcdl.cpp\
               src/ffu.cpp\
               src/firehose.cpp\
               src/partition.cpp\
               src/protocol.cpp\
               src/sahara.cpp\
               src/serialport.cpp\
               src/sparse.cpp\
               src/xmlparser.cpp

if SYSDEPS_WIN32
emmcdl_SOURCES += \
               src/writedisk_windows.cpp
else
emmcdl_SOURCES += \
               src/writedisk_linux.cpp
endif
