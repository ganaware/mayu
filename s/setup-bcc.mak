############################################################## -*- Makefile -*-
#
# Makefile for mayu (Borland C++ 5.5)
#
#	make release version: make nodebug=1
#	make debug version: make
#
###############################################################################


!include <../bcc.mak>
!include <setup-common.mak>

LDFLAGS_1	= $(guilflags) $(guiobjs)
LDFLAGS_2	= $(conlflags) $(conobjs)

$(TARGET_1):	$(OBJS_1) $(RES_1) $(EXTRADEP_1)
	$(LD) $(LDFLAGS_1) $(OBJS_1),$(TARGET_1),,$(LIBS_1),,$(RES_1)

$(TARGET_2):	$(OBJS_2) $(RES_2) $(EXTRADEP_2)
	$(LD) $(LDFLAGS_2) $(OBJS_2),$(TARGET_2),,$(LIBS_2) cab32-bcc.lib,,\
		$(RES_2)

strres.h:	setup.rc
	cmd /c grep -q- IDS setup.rc | \
	sed "s/\(IDS[a-zA-Z0-9_]*\)[^""]*\("".*\)$$/\1, \2,/" | \
	sed "s/^File.*$//" > strres.h
