############################################################## -*- Makefile -*-
#
# Makefile for setup (Visual C++ 6.0)
#
#	make release version: nmake nodebug=1
#	make debug version: nmake
#
###############################################################################


!include <..\vc.mak>
!include <setup-common.mak>

LDFLAGS_1	= $(guilflags)
LDFLAGS_2	= $(conlflags)

$(TARGET_1):	$(OBJS_1) $(RES_1) $(EXTRADEP_1)
	$(link) -out:$@ $(ldebug) $(LDFLAGS_1) $(OBJS_1) $(LIBS_1) $(RES_1)

$(TARGET_2):	$(OBJS_2) $(RES_2) $(EXTRADEP_2)
	$(link) -out:$@ $(ldebug) $(LDFLAGS_2) $(OBJS_2) $(LIBS_2) $(RES_2) \
		cab32-vc.lib

strres.h:	setup.rc
	grep IDS setup.rc | \
	sed "s/\(IDS[a-zA-Z0-9_]*\)[^""]*\("".*\)$$/\1, \2,/" > strres.h
