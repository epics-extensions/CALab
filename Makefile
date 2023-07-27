# Linux Makefile
#
# Modify these to match your installation directories
LVDIR ?= /usr/local/natinst/LabVIEW-2020-64/cintools
EPICSDIR ?= /usr/local/epics/include

# You probably don't have to change anything below
CC ?= g++
# DEBUGFLAGS=-D_DEBUG
CFLAGS ?= -fPIC -std=c++17 -g3 -Wall -Wno-multichar $(DEBUGFLAGS)
INCLUDES ?= -I$(LVDIR) -I$(EPICSDIR) -I$(EPICSDIR)/os/Linux

all:	libcalab.so

calab.o:	src/calab.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -c src/calab.cpp

libcalab.so:	calab.o
	$(CC) -fPIC -L$(LVDIR) -shared -o libcalab.so calab.o

clean:
	rm -f calab.o libcalab.so
