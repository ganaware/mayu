############################################################## -*- Makefile -*-
#
# Makefile for setup
#
###############################################################################


!if "$(TARGETOS)" == "WINNT"
OS_SPECIFIC_DEFINES	=  -DUNICODE -D_UNICODE
DISTRIB_OS	= nt
!endif
!if "$(TARGETOS)" == "WIN95"
OS_SPECIFIC_DEFINES	=  -D_MBCS
DISTRIB_OS	= 9x
!endif
!if "$(TARGETOS)" == "BOTH"
!error Must specify TARGETOS=WIN95 or TARGETOS=WINNT
!endif


DEFINES		= -DSTRICT -D_WIN32_IE=0x0400 $(OS_SPECIFIC_DEFINES) \
		  $(DEBUGDEFINES)
BOOST_DIR	= ../../boost


# setup.exe	###############################################################

TARGET_1	= setup.exe
OBJS_1		=			\
		setup.obj		\
		installer.obj		\
		..\registry.obj		\
		..\stringtool.obj	\
		..\windowstool.obj	\

RES_1		= setup.res

LIBS_1		= $(guixlibsmt) shell32.lib ole32.lib uuid.lib


# cab32.exe	###############################################################

TARGET_2	= cab32.exe
OBJS_2		= cab32.obj
LIBS_2		= $(conxlibsmt)


# rules		###############################################################

all:		$(TARGET_1) $(TARGET_2)

setup.cpp:	strres.h

clean:
		-$(RM) *.obj
		-$(RM) $(TARGET_1) strres.h
		-$(RM) *.res
		-$(RM) *~ $(CLEAN)
