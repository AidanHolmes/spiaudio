AMINETNAME = SPIAudio
DEVICENAME = mhispiaudio
# Version - these values are mastered here and overwrite the generated values in makefiles for Debug and Release
LIBDEVMAJOR = 1
LIBDEVMINOR = 1
LIBDEVDATE = "03.01.2026"
LIBDEVNAME = $(DEVICENAME).library

LHADIR = $(AMINETNAME)
RELEASEDIR = Release
DEBUGDIR = Debug
RELEASE = $(RELEASEDIR)/makefile
DEBUG = $(DEBUGDIR)/makefile

# optimised and release version
#optdepth
# defines the maximum depth of function calls to be Mined. The
# range is 0 to 6, and the default value is 3.
PRODCOPTS = NOSTACKCHECK OPTIMIZE Optimizerinline OptimizerInLocal OptimizerLoop OptimizerComplexity=30 OptimizerGlobal OptimizerDepth=6 OptimizerTime OptimizerSchedule OptimizerPeephole

# debug version build options
DBGCOPTS = DEFINE=_DEBUG DEFINE=DEBUG_SERIAL debug=full NOSTACKCHECK

all: $(RELEASE) $(DEBUG) $(AMINETNAME).lha includes
	execute <<
		cd $(RELEASEDIR)
		smake LIBDEVMAJOR=$(LIBDEVMAJOR) LIBDEVMINOR=$(LIBDEVMINOR) LIBDEVDATE=$(LIBDEVDATE) LIBDEVNAME=$(LIBDEVNAME) DEVICENAME=$(DEVICENAME)
		cd /
		<
	execute <<
		cd $(DEBUGDIR)
		smake LIBDEVMAJOR=$(LIBDEVMAJOR) LIBDEVMINOR=$(LIBDEVMINOR) LIBDEVDATE=$(LIBDEVDATE) LIBDEVNAME=$(LIBDEVNAME) DEVICENAME=$(DEVICENAME)
		cd /
		<

lib: $(AMINETNAME).lha

install: $(AMINETNAME).lha
	copy $(DEVICENAME)/$(LIBDEVNAME) DEVS:
	
clean:
	execute <<
		cd $(RELEASEDIR)
		smake clean LIBDEVNAME=$(LIBDEVNAME) DEVICENAME=$(DEVICENAME)
		cd /
		<
	execute <<
		cd $(DEBUGDIR)
		smake clean LIBDEVNAME=$(LIBDEVNAME) DEVICENAME=$(DEVICENAME)
		cd /
		<
	delete $(AMINETNAME).lha $(AMINETNAME)/$(LIBDEVNAME) $(AMINETNAME)/Include/C/pragma/\#? $(AMINETNAME)/Include/C/inline/\#? $(AMINETNAME)/Include/C/proto/\#? $(AMINETNAME)/Include/C/inline/\#? $(AMINETNAME)/Include/Assembly/lvo/\#? $(AMINETNAME)/FD/\#?
	
$(RELEASE): makefile.master makefile
	copy makefile.master $(RELEASE)
	splat -o "^SCOPTS.+\$" "SCOPTS = $(PRODCOPTS)" $(RELEASE)
	splat -o "^DEVICENAME.+\$" "DEVICENAME = $(DEVICENAME)" $(RELEASE)
	splat -o "^LIBDEVMAJOR.+\$" "LIBDEVMAJOR = $(LIBDEVMAJOR)" $(RELEASE)
	splat -o "^LIBDEVMINOR.+\$" "LIBDEVMINOR = $(LIBDEVMINOR)" $(RELEASE)
#	splat -o "^LIBDEVDATE.+\$" "LIBDEVDATE = $(LIBDEVDATE)" $(RELEASE)
	splat -o "^BUILD.+\$" "BUILD = Release" $(RELEASE)
	splat -o "^AMINETNAME.+\$" "AMINETNAME = $(AMINETNAME)" $(RELEASE)
	
$(DEBUG): makefile.master makefile
	copy makefile.master $(DEBUG)
	splat -o "^SCOPTS.+\$" "SCOPTS = $(DBGCOPTS)" $(DEBUG)
	splat -o "^DEVICENAME.+\$" "DEVICENAME = $(DEVICENAME)" $(DEBUG)
	splat -o "^LIBDEVMAJOR.+\$" "LIBDEVMAJOR = $(LIBDEVMAJOR)" $(DEBUG)
	splat -o "^LIBDEVMINOR.+\$" "LIBDEVMINOR = $(LIBDEVMINOR)" $(DEBUG)
#	splat -o "^LIBDEVDATE.+\$" "LIBDEVDATE = $(LIBDEVDATE)" $(DEBUG)
	splat -o "^BUILD.+\$" "BUILD = Debug" $(DEBUG)
	splat -o "^AMINETNAME.+\$" "AMINETNAME = $(AMINETNAME)" $(DEBUG)
	
$(AMINETNAME).lha: $(RELEASE)
	execute <<
		cd $(RELEASEDIR)
		smake lib LIBDEVMAJOR=$(LIBDEVMAJOR) LIBDEVMINOR=$(LIBDEVMINOR) LIBDEVDATE=$(LIBDEVDATE) LIBDEVNAME=$(LIBDEVNAME) DEVICENAME=$(DEVICENAME)
		cd /
		<
	lha -Qr -xr u $(AMINETNAME).lha $(LHADIR)
	
includes: Src/SFD/spiaudio_lib.sfd
	fd2pragma "Src/SFD/spiaudio_lib.sfd" 111 TO "$(AMINETNAME)/Include/C/clib" AUTOHEADER
	fd2pragma "Src/SFD/spiaudio_lib.sfd" 110 TO "$(AMINETNAME)/FD"
	fd2pragma "Src/SFD/spiaudio_lib.sfd" 6 TO "$(AMINETNAME)/Include/C/pragma"
	fd2pragma "Src/SFD/spiaudio_lib.sfd" 40 TO "$(AMINETNAME)/Include/C/inline"
	fd2pragma "Src/SFD/spiaudio_lib.sfd" 38 TO "$(AMINETNAME)/Include/C/proto"
	fd2pragma "Src/SFD/spiaudio_lib.sfd" 70 TO "$(AMINETNAME)/Include/C/inline"
	fd2pragma "Src/SFD/spiaudio_lib.sfd" 23 TO "$(AMINETNAME)/Include/Assembly/lvo"
	