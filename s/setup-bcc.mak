############################################################## -*- Makefile -*-
#
# Makefile for mayu (Borland C++ 5.5)
#
#	make release version: make nodebug=1
#	make debug version: make
#
###############################################################################


!if "$(BOOST_VER)" == ""
BOOST_VER	= 1_31
!endif
INCLUDES	= -I$(BOOST_DIR)
DEPENDIGNORE	= --ignore=$(BOOST_DIR)

!include <../bcc.mak>
!include <setup-common.mak>

LDFLAGS_1	= $(guilflags) $(guiobjs) -L"$(BOOST_DIR)\libs\regex\build\bcb"

$(TARGET_1):	$(OBJS_1) $(RES_1) $(EXTRADEP_1)
	$(LD) $(LDFLAGS_1) $(OBJS_1),$(TARGET_1),,$(LIBS_1),,$(RES_1)

strres.h:	setup.rc
	cmd /c grep IDS setup.rc | \
	sed "s/\(IDS[a-zA-Z0-9_]*\)[^""]*\("".*\)$$/ \1, _T(\2) ,/" | \
	sed "s/^File.*$//" > strres.h

batch:
		-$(MAKE) -f setup-bcc.mak TARGETOS=WINNT nodebug=1
		-$(MAKE) -f setup-bcc.mak TARGETOS=WINNT
		-$(MAKE) -f setup-bcc.mak TARGETOS=WIN95 nodebug=1
		-$(MAKE) -f setup-bcc.mak TARGETOS=WIN95

batch_clean:
		-$(MAKE) -f setup-bcc.mak TARGETOS=WINNT nodebug=1 clean
		-$(MAKE) -f setup-bcc.mak TARGETOS=WINNT clean
		-$(MAKE) -f setup-bcc.mak TARGETOS=WIN95 nodebug=1 clean
		-$(MAKE) -f setup-bcc.mak TARGETOS=WIN95 clean
