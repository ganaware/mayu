############################################################## -*- Makefile -*-
#
# Makefile (Visual C++ 6.0)
#
#	make release version: nmake nodebug=1
#	make debug version: nmake
#
###############################################################################


# VC++ rules	###############################################################

APPVER		= 5.0
#TARGETOS	= WINNT
#TARGETLANG	= LANG_JAPANESE
_WIN32_IE	= 0x0500
!include <win32.mak>

!ifdef nodebug
DEBUG_FLAG	= -DNDEBUG
!else
DEBUG_FLAG	=
!endif

.cpp.obj:
	$(cc) -GX $(cdebug) $(cflags) $(cvarsmt) $(DEFINES) $(INCLUDES) \
		$(DEBUG_FLAG) -Fo$@ $*.cpp
.rc.res:
	$(rc) $(rcflags) $(rcvars) $*.rc

conxlibsmt	= $(conlibsmt) libcpmt.lib
guixlibsmt	= $(guilibsmt) libcpmt.lib

DEPENDFLAGS	= --cpp=vc --ignore="$(INCLUDE)" $(DEPENDIGNORE) \
		-GX $(cdebug) $(cflags) $(cvarsmt) $(DEFINES) $(INCLUDES) \
		$(DEBUG_FLAG)


# tools		###############################################################

RM		= del
COPY		= copy
ECHO		= echo
