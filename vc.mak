############################################################## -*- Makefile -*-
#
# Makefile (Visual C++ 6.0)
#
#	make release version: nmake nodebug=1
#	make debug version: nmake
#
###############################################################################


# VC++ rules	###############################################################

APPVER		= 4.0
TARGETOS	= WINNT
!include <win32.mak>

!ifdef nodebug
DEBUG_FLAG	= -DNDEBUG
!else
DEBUG_FLAG	=
!endif

.cpp.obj:
	$(cc) -GX $(cdebug) $(cflags) $(cvarsmt) $(DEFINES) $(DEBUG_FLAG) \
		-Fo$@ $*.cpp
.rc.res:
	$(rc) $(rcflags) $(rcvars) $*.rc

conxlibsmt	= $(conlibsmt) libcpmt.lib
guixlibsmt	= $(guilibsmt) libcpmt.lib


# tools		###############################################################

RM		= del
COPY		= copy
ECHO		= echo
