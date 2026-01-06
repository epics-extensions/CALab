# Linux Makefile

# Modify these to match your installation directories
LVDIR     ?= /usr/local/natinst/LabVIEW-2024-64/cintools
EPICSDIR  ?= /usr/local/epics/include

# You probably don't have to change anything below
CXX       ?= g++
CXXFLAGS  ?= -fPIC -std=c++17 -Wall -Wextra -Wno-multichar
CXXFLAGS  += -fexceptions -frtti -fno-lto
CXXFLAGS  += -MMD -MP
BUILD     ?= release

ifeq ($(BUILD),debug)
  CXXFLAGS += -g3 -O0
else
  CXXFLAGS += -O2
endif

INCLUDES  ?= -I$(LVDIR) -I$(EPICSDIR) -I$(EPICSDIR)/os/Linux -I$(EPICSDIR)/compiler/gcc
LDFLAGS   ?= -shared -fno-lto
LDLIBS    ?= -ldl -lpthread

SRC = $(wildcard src/*.cpp)
OBJ = $(SRC:.cpp=.o)
DEPS = $(OBJ:.o=.d)
TARGET = libcalab.so
DESTDIR = /usr/local/calab

all: $(TARGET)
	@echo "Copying $(TARGET) to $(DESTDIR)/"
	@if [ "$(abspath $(DESTDIR))" != "$(abspath $(CURDIR))" ]; then \
		mkdir -p "$(DESTDIR)"; \
		cp -f "$(TARGET)" "$(DESTDIR)/"; \
	else \
		echo "Target is current directory; skipping copy."; \
	fi

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(TARGET): $(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

-include $(DEPS)

clean:
	rm -f $(OBJ) $(DEPS) $(TARGET)
