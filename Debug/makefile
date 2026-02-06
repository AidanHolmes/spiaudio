# Version 
AMINETNAME = SPIAudio
DEVICENAME = mhispiaudio
LIBDEVMAJOR = 1
LIBDEVMINOR = 6
LIBDEVDATE = "1.1.2025"

LIBDEVNAME = $(DEVICENAME).device

BUILD = Debug
LIBTESTBIN = testlib

TESTBIN = audio-test

VISUALBIN = spidervis

NDKHOME=//lib/NDK3.2R4
SPIDERHOME=//lib/spiderdev
SPIDERLIBH=$(SPIDERHOME)/Src
SPIDERLIBBUILD=$(SPIDERHOME)/$(BUILD)/Bin/spiderdev.lib
AMIGAUILIB=//lib/amigaui
AMIGAUILIBH=$(AMIGAUILIB)/Src
AMIGAUILIBBUILD=$(AMIGAUILIB)/$(BUILD)/Bin/ui.lib
MHI=//lib/mhi_dev
MHI_H=$(MHI)/Include

SRC = /Src
BIN = Bin
OBJ = Obj
OBJSAUD = $(OBJ)/timing.o $(OBJ)/debug.o $(OBJ)/vs1053.o
OBJSLIB = $(OBJ)/libdev.o $(OBJ)/spiaudiolib.o 
OBJSBIN = $(OBJ)/$(TESTBIN).o
OBJSVIS = $(OBJ)/$(VISUALBIN).o
OBJSLIBBIN = $(OBJ)/$(LIBTESTBIN).o $(OBJ)/timing.o $(OBJ)/debug.o

EXTRA_H = IncludeDirectory=$(SPIDERLIBH) IncludeDirectory=$(MHI_H) IncludeDirectory=$(AMIGAUILIBH) IncludeDirectory=$(NDKHOME)/Include_H
DEFSTD = DEFINE=__NOLIBBASE__ DEFINE=AS_LIBRARY DEFINE=LIBDEVMAJOR=$(LIBDEVMAJOR) DEFINE=LIBDEVMINOR=$(LIBDEVMINOR) DEFINE=LIBDEVDATE=$(LIBDEVDATE) DEFINE=LIBDEVNAME=$(LIBDEVNAME)

# parameters
SCOPTS = nocheckabort DEFINE=VS10XX_USE_INTERRUPTS=1 DEFINE=_DEBUG DEFINE=DEBUG_SERIAL debug=full NOSTACKCHECK
all: dirs $(BIN)/$(TESTBIN) $(BIN)/$(LIBDEVNAME) $(BIN)/$(LIBTESTBIN) $(BIN)/$(VISUALBIN)

dirs:
	- makedir $(BIN)
	- makedir $(OBJ)
	
clean:
   - delete $(OBJ)/\#?.o $(OBJ)/\#?.asm $(BIN)/\#?.lnk $(BIN)/\#?.info $(BIN)/\#?.map $(BIN)/\#?.gst $(BIN)/$(LIBDEVNAME) $(BIN)/$(TESTBIN) $(BIN)/$(LIBTESTBIN) $(BIN)/$(VISUALBIN)

install:
	- makedir LIBS:MHI
	copy $(BIN)/$(LIBDEVNAME) LIBS:MHI/
	
lib: $(BIN)/$(LIBDEVNAME) $(BIN)/$(VISUALBIN)
	copy $(BIN)/$(LIBDEVNAME) /$(AMINETNAME)/$(LIBDEVNAME)
	copy $(BIN)/$(VISUALBIN) /$(AMINETNAME)/$(VISUALBIN)

$(BIN)/$(LIBDEVNAME): $(OBJSLIB) $(OBJSAUD)
	sc link to $@ $(OBJSLIB) $(OBJSAUD) Library=$(SPIDERLIBBUILD)
	
$(BIN)/$(TESTBIN): $(OBJSAUD) $(OBJSBIN)
	sc link to $@ $(OBJSAUD) $(OBJSBIN) Library=$(SPIDERLIBBUILD)
	
$(BIN)/$(LIBTESTBIN): $(OBJSLIBBIN)
	sc link to $@ $(OBJSLIBBIN)
	
$(BIN)/$(VISUALBIN): $(OBJSVIS)
	sc link to $@ $(OBJSVIS) Library=$(AMIGAUILIBBUILD)
	
.c.o:
	sc $(SCOPTS) $(DEFSTD) $(EXTRA_H) $? ObjectName=$(OBJ)/
	
.a.o:
	sc $? ObjectName=$(OBJ)/
   
$(OBJ)/timing.o: $(SRC)/timing.c 

$(OBJ)/spiaudiolib.o: $(SRC)/spiaudiolib.c 

$(OBJ)/debug.o: $(SRC)/debug.c 

$(OBJ)/vs1053.o: $(SRC)/vs1053.c

$(OBJ)/libdev.o: $(SRC)/libdev.c

$(OBJ)/$(TESTBIN).o: $(SRC)/$(TESTBIN).c

$(OBJ)/$(LIBTESTBIN).o: $(SRC)/$(LIBTESTBIN).c

$(OBJ)/$(VISUALBIN).o: $(SRC)/$(VISUALBIN).c