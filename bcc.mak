############################################################## -*- Makefile -*-
#
# Makefile (Borland C++ 5.5)
#
#	make release version: make nodebug=1
#	make debug version: make
#
###############################################################################


# BCC rules	###############################################################

# Please edit your C:\Borland\bcc55\bin\{bcc32.cfg,ilink32.cfg} to
# add psdk library path:
# -------- bcc32.cfg
# -I"C:\Borland\Bcc55\include"
# -L"C:\Borland\Bcc55\lib"
# -L"C:\Borland\Bcc55\lib\psdk"
# -------- ilink32.cfg
# -L"c:\Borland\Bcc55\lib" -L"c:\Borland\Bcc55\lib\psdk"

LD		= ilink32.exe
IMPLIB		= implib.exe

CLEAN		= *.ilc *.ild *.ilf *.ils *.map *.tds

# compiler flags

!ifdef nodebug
DEBUG_FLAG	= -DNDEBUG
cdebug		= -d -O1
cvarsmt		= -tWM
!else
DEBUG_FLAG	=
cdebug		= -d -Od -v -w
cvarsmt		= -tWM
!endif

CFLAGS		= $(cdebug) $(cvarsmt) $(DEFINES) $(DEBUG_FLAG)
CPPFLAGS	= $(cdebug) $(cvarsmt) $(DEFINES) $(DEBUG_FLAG)

# linker flags

conlflags	= -x -ap
guilflags	= -x -aa
dlllflags	= -x -Tpd

conobjs		= c0x32.obj
guiobjs		= c0w32.obj
dllobjs		= c0d32.obj

conlibs		= import32.lib cw32.lib
guilibs		= import32.lib cw32.lib

conlibsmt	= import32.lib cw32mt.lib
guilibsmt	= import32.lib cw32mt.lib

conxlibsmt	= $(conlibsmt)
guixlibsmt	= $(guilibsmt)

# rule

.dll.lib:
	$(IMPLIB) $@ $**


# tools		###############################################################

RM		= cmd /c del
COPY		= cmd /c copy
ECHO		= cmd /c echo
