############################################################## -*- Makefile -*-
#
# Makefile (Borland C++ 5.5)
#
#	make release version: make nodebug=1
#	make debug version: make
#
###############################################################################
#
# Please edit your C:\Borland\bcc55\bin\{bcc32.cfg,ilink32.cfg} to
# add psdk library path:
# -------- bcc32.cfg
# -I"C:\Borland\Bcc55\include"
# -L"C:\Borland\Bcc55\lib"
# -L"C:\Borland\Bcc55\lib\psdk"
# -------- ilink32.cfg
# -L"c:\Borland\Bcc55\lib" -L"c:\Borland\Bcc55\lib\psdk"
#
###############################################################################


# BCC rules	###############################################################

!if "$(TARGETOS)" == ""
TARGETOS	= WINNT
!endif	# TARGETOS

!if "$(TARGETOS)" == "WINNT"
APPVER		= 4.0
!ifdef nodebug
OUT_DIR		= outbcc_winnt
!else	# nodebug
OUT_DIR		= outbcc_winnt_debug
!endif	# nodebug
!endif	# TARGETOS

!if "$(TARGETOS)" == "WIN95"
APPVER		= 4.0
!ifdef nodebug
OUT_DIR		= outbcc_win9x
!else	# nodebug
OUT_DIR		= outbcc_win9x_debug
!endif	# nodebug
!endif	# TARGETOS

!if "$(TARGETOS)" == "BOTH"
!error Must specify TARGETOS=WIN95 or TARGETOS=WINNT
!endif	# TARGETOS

# compilers

LD		= ilink32.exe
IMPLIB		= implib.exe

CLEAN		= $(OUT_DIR)\*.il? $(OUT_DIR)\*.map $(OUT_DIR)\*.tds

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

CFLAGS		= $(cdebug) $(cvarsmt) $(INCLUDES) $(DEBUG_FLAG) \
		$(DEFINES) -D_$(TARGETOS)
CPPFLAGS	= $(cdebug) $(cvarsmt) $(DEBUG_FLAG) $(INCLUDES) \
		$(DEFINES) -D_$(TARGETOS)

# linker flags

conlflags	= -x -ap
guilflags	= -x -aa
dlllflags	= -x -Tpd

!if "$(TARGETOS)" == "WINNT"
conobjs		= c0x32w.obj
guiobjs		= c0w32w.obj
dllobjs		= c0d32w.obj
!endif # TARGETOS

!if "$(TARGETOS)" == "WIN95"
conobjs		= c0x32.obj
guiobjs		= c0w32.obj
dllobjs		= c0d32.obj
!endif # TARGETOS

conlibs		= import32.lib cw32.lib
guilibs		= import32.lib cw32.lib

conlibsmt	= import32.lib cw32mt.lib
guilibsmt	= import32.lib cw32mt.lib

conxlibsmt	= $(conlibsmt)
guixlibsmt	= $(guilibsmt)

DEPENDFLAGS	= --cpp=bcc32 -p"$$(OUT_DIR)\\" $(DEPENDIGNORE) $(CPPFLAGS)

# rule

.dll.lib:
	$(IMPLIB) $@ $**

{}.cpp{$(OUT_DIR)}.obj:
	$(CC) $(CPPFLAGS) -n$(OUT_DIR) /c $&.cpp

{}.rc{$(OUT_DIR)}.res:
	$(RC) $(RFLAGS) /r /istub $&
	$(MV) $&.res $(OUT_DIR)\$&.res


# tools		###############################################################

RM		= cmd /c del
MV		= cmd /c move
COPY		= cmd /c copy
ECHO		= cmd /c echo
MKDIR		= cmd /c mkdir
RMDIR		= cmd /c rmdir
