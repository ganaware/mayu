############################################################## -*- Makefile -*-
#
# Makefile for mayu (Borland C++ 5.5)
#
#	make release version: make nodebug=1
#	make debug version: make
#
###############################################################################


!include <bcc.mak>
!include <mayu-common.mak>

DEFINES		= -DSTRICT -DVERSION="\"$(VERSION)\""

LDFLAGS_1	= $(guilflags) $(guiobjs)
LDFLAGS_2	= $(dlllflags) $(dllobjs)

$(TARGET_1):	$(OBJS_1) $(RES_1) $(EXTRADEP_1)
	$(LD) $(LDFLAGS_1) $(OBJS_1),$(TARGET_1),,$(LIBS_1),,$(RES_1)

$(TARGET_2):	$(OBJS_2) $(RES_2) $(EXTRADEP_2)
	$(LD) $(LDFLAGS_2) $(OBJS_2),$(TARGET_2),,$(LIBS_2),,$(RES_2)

$(TARGET_3):	$(DLL_3)
