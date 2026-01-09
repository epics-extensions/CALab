# Linux Makefile (robust for debugging + sanitizers)

# Installation/include paths (as in your setup)
LVDIR     ?= /usr/local/natinst/LabVIEW-2024-64/cintools
EPICSDIR  ?= /usr/local/epics/include

CXX       ?= g++
BUILD     ?= release

DESTDIR   ?= /usr/local/calab
TARGET_BASENAME := libcalab.so
# Stable symlink name in repo root to the last built variant
LINK      := $(TARGET_BASENAME)

# Sources
SRC := $(wildcard src/*.cpp)

# Out-of-tree build
OUTDIR := build/$(BUILD)
OBJDIR := $(OUTDIR)/obj
TARGET := $(OUTDIR)/$(TARGET_BASENAME)
# Use an absolute path to keep the symlink valid from any working directory
LINK_TARGET := $(abspath $(TARGET))

OBJ  := $(patsubst src/%.cpp,$(OBJDIR)/%.o,$(SRC))
DEPS := $(OBJ:.o=.d)

# Includes
INCLUDES  := -I$(LVDIR) -I$(EPICSDIR) -I$(EPICSDIR)/os/Linux -I$(EPICSDIR)/compiler/gcc

# Common flags
CXXFLAGS_COMMON := -fPIC -std=c++17 -Wall -Wextra -Wno-multichar
CXXFLAGS_COMMON += -fexceptions -frtti -fno-lto
CXXFLAGS_COMMON += -MMD -MP

LDFLAGS_COMMON  := -shared -fno-lto
LDLIBS          := -ldl -lpthread

# Build-specific flags
CXXFLAGS_BUILD :=
LDFLAGS_BUILD  :=

ifeq ($(BUILD),debug)
  CXXFLAGS_BUILD += -g3 -O0 -fno-omit-frame-pointer
  CXXFLAGS_BUILD += -DDEBUG
else ifeq ($(BUILD),relwithdebinfo)
  CXXFLAGS_BUILD += -g3 -O2 -fno-omit-frame-pointer
  CXXFLAGS_BUILD += -DNDEBUG
else ifeq ($(BUILD),asan)
  # ASan/UBSan: -O1 is often a good tradeoff (performance + reproducibility)
  CXXFLAGS_BUILD += -g3 -O1 -fno-omit-frame-pointer
  CXXFLAGS_BUILD += -fsanitize=address,undefined
  LDFLAGS_BUILD  += -fsanitize=address,undefined
else
  # release
  CXXFLAGS_BUILD += -O2 -DNDEBUG
endif

CXXFLAGS := $(CXXFLAGS_COMMON) $(CXXFLAGS_BUILD)
LDFLAGS  := $(LDFLAGS_COMMON) $(LDFLAGS_BUILD)

.PHONY: all install clean print link

all: link

link: $(TARGET)
	@echo "Linking $(LINK) -> $(LINK_TARGET)"
	@ln -sfn "$(LINK_TARGET)" "$(LINK)"

install: $(TARGET)
	@echo "Installing $(TARGET_BASENAME) -> $(DESTDIR)/"
	@mkdir -p "$(DESTDIR)"
	@cp -f "$(TARGET)" "$(DESTDIR)/$(TARGET_BASENAME)"

print:
	@echo "BUILD=$(BUILD)"
	@echo "TARGET=$(TARGET)"
	@echo "CXXFLAGS=$(CXXFLAGS)"
	@echo "LDFLAGS=$(LDFLAGS)"

$(OBJDIR)/%.o: src/%.cpp
	@mkdir -p "$(OBJDIR)"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(TARGET): $(OBJ)
	@mkdir -p "$(OUTDIR)"
	$(CXX) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

-include $(DEPS)

clean:
	rm -rf build
