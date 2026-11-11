# Makefile for irrbigmap (Linux)
#
# Needs the Irrlicht 1.8 headers/library and the Boost.Thread headers/library:
#   Debian/Ubuntu:  sudo apt install g++ make libirrlicht-dev libboost-thread-dev
#
# Usage:
#   make            build ../bin/bigmap
#   make run        build, then run it from ../media
#   make overview   generate the distant-terrain images (needs Python, numpy, Pillow)
#   make clean
#
# The Irrlicht/Boost locations can be overridden, e.g.:
#   make IRRLICHT_INC=$HOME/irrlicht-1.8/include IRRLICHT_LIB=$HOME/irrlicht-1.8/lib/Linux

Target       = bigmap
Sources      = TiledTerrain.cpp OverviewTerrain.cpp main.cpp
BIN_DIR      = ../bin
MEDIA_DIR    = ../media

CXX         ?= g++
IRRLICHT_INC ?= /usr/include/irrlicht
IRRLICHT_LIB ?=

CXXFLAGS    ?= -O3 -ffast-math -Wall
CPPFLAGS    += -isystem $(IRRLICHT_INC)
LDFLAGS     += $(if $(IRRLICHT_LIB),-L$(IRRLICHT_LIB)) -lIrrlicht -lboost_thread

all: $(BIN_DIR)/$(Target)

$(BIN_DIR)/$(Target): $(Sources) TiledTerrain.h OverviewTerrain.h
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(Sources) -o $@ $(LDFLAGS)

# The program loads "media/..." relative to the working directory.
run: $(BIN_DIR)/$(Target)
	cd $(MEDIA_DIR) && $(abspath $(BIN_DIR)/$(Target))

# Builds media/overview_height.png and media/overview_color.jpg from the tile zips.
overview:
	cd .. && python3 tools/make_overview.py --media media

clean:
	$(RM) $(BIN_DIR)/$(Target)

.PHONY: all run overview clean
