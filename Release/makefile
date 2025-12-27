# Version 
AMINETNAME = SPIAudio
DEVICENAME = mhispiaudio
LIBDEVMAJOR = 1
LIBDEVMINOR = 0
LIBDEVDATE = "1.1.2025"

LIBDEVNAME = $(DEVICENAME).device

BUILD = Release
LIBTESTBIN = testlib

TESTBIN = audio-test

SPIDERHOME=//lib/spiderdev
SPIDERLIBH=$(SPIDERHOME)/Src
SPIDERLIBBUILD=$(SPIDERHOME)/$(BUILD)/Bin/spiderdev.lib
MHI=//lib/mhi_dev
MHI_H=$(MHI)/Include

SRC = /Src/
BIN = Bin/
OBJ = Obj/
#OBJSAUD = $(OBJ)timing.o $(OBJ)debug.o $(OBJ)vs1053.o $(OBJ)fncasm.o $(OBJ)spi.o $(OBJ)config_file.o
OBJSAUD = $(OBJ)timing.o $(OBJ)debug.o $(OBJ)vs1053.o
OBJSLIB = $(OBJ)libdev.o $(OBJ)spiaudiolib.o 
OBJSBIN = $(OBJ)$(TESTBIN).o
OBJSLIBBIN = $(OBJ)$(LIBTESTBIN).o

EXTRA_H = IncludeDirectory=$(SPIDERLIBH) IncludeDirectory=$(MHI_H)
DEFSTD = DEFINE=AS_LIBRARY DEFINE=LIBDEVMAJOR=$(LIBDEVMAJOR) DEFINE=LIBDEVMINOR=$(LIBDEVMINOR) DEFINE=LIBDEVDATE=$(LIBDEVDATE) DEFINE=LIBDEVNAME="$(LIBDEVNAME)"

# parameters
SCOPTS = NOSTACKCHECK OPTIMIZE Optimizerinline OptimizerInLocal OptimizerLoop OptimizerComplexity=30 OptimizerGlobal OptimizerDepth=6 OptimizerTime OptimizerSchedule OptimizerPeephole
all: $(BIN)$(TESTBIN) $(BIN)$(LIBDEVNAME) $(BIN)$(LIBTESTBIN)

clean:
   - delete $(OBJ)\#?.o $(OBJ)\#?.asm $(BIN)\#?.lnk $(BIN)\#?.info $(BIN)\#?.map $(BIN)\#?.gst $(BIN)$(LIBDEVNAME) $(BIN)$(TESTBIN) $(BIN)$(LIBTESTBIN)

install:
	- copy $(BIN)$(LIBDEVNAME) DEVS:
	
lib: $(BIN)$(LIBDEVNAME)
	copy $? /$(AMINETNAME)/$(LIBDEVNAME)

$(BIN)$(LIBDEVNAME): $(OBJSLIB) $(OBJSAUD)
	sc link to $@ $(OBJSLIB) $(OBJSAUD) Library=$(SPIDERLIBBUILD)
	
$(BIN)$(TESTBIN): $(OBJSAUD) $(OBJSBIN)
	sc link to $@ $(OBJSAUD) $(OBJSBIN) Library=$(SPIDERLIBBUILD)
	
$(BIN)$(LIBTESTBIN): $(OBJSLIBBIN)
	sc link to $@ $(OBJSLIBBIN)
	
.c.o:
	sc $(SCOPTS) $(DEFSTD) $(EXTRA_H) $? ObjectName=$(OBJ)
	
.a.o:
	sc $? ObjectName=$(OBJ)
   
$(OBJ)timing.o: $(SRC)timing.c 

$(OBJ)spiaudiolib.o: $(SRC)spiaudiolib.c 

$(OBJ)debug.o: $(SRC)debug.c 

$(OBJ)vs1053.o: $(SRC)vs1053.c

$(OBJ)libdev.o: $(SRC)libdev.c

$(OBJ)$(TESTBIN).o: $(SRC)$(TESTBIN).c

#$(OBJ)fncasm.o: $(SRC)fncasm.a

#$(OBJ)spi.o: $(SRC)spi.c 

#$(OBJ)config_file.o: $(SRC)config_file.c 

$(OBJ)$(LIBTESTBIN).o: $(SRC)$(LIBTESTBIN).c