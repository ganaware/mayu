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

!include <bcc.mak>
!include <mayu-common.mak>

DEFINES		= $(COMMON_DEFINES) -DVERSION="\"$(VERSION)\"" \
		-DLOGNAME="\"$(LOGNAME)\"" -DCOMPUTERNAME="\"$(COMPUTERNAME)\""

LDFLAGS_1	= $(guilflags) $(guiobjs) -L"$(BOOST_DIR)\libs\regex\build\bcb"
LDFLAGS_2	= $(dlllflags) $(dllobjs) -L"$(BOOST_DIR)\libs\regex\build\bcb"

$(TARGET_1):	$(OBJS_1) $(RES_1) $(EXTRADEP_1)
	$(LD) $(LDFLAGS_1) $(OBJS_1),$(TARGET_1),,$(LIBS_1),,$(RES_1)

$(TARGET_2):	$(OBJS_2) $(RES_2) $(EXTRADEP_2)
	$(LD) $(LDFLAGS_2) $(OBJS_2),$(TARGET_2),,$(LIBS_2),,$(RES_2)

$(TARGET_3):	$(DLL_3)

boost:
		cd $(BOOST_DIR)/libs/regex/build/
		$(MAKE) -i -f bcb6.mak bcb bcb\libboost_regex-bcb-mt-s-$(BOOST_VER) bcb\libboost_regex-bcb-mt-s-$(BOOST_VER).lib
		cd ../../../../mayu

distclean::	clean
		cd $(BOOST_DIR)/libs/regex/build/
		-$(MAKE) -i -f bcb6.mak clean
		cd ../../../../mayu

batch:
		-$(MAKE) -i -f mayu-bcc.mak TARGETOS=WINNT nodebug=1
		-$(MAKE) -i -f mayu-bcc.mak TARGETOS=WINNT
		-$(MAKE) -i -f mayu-bcc.mak TARGETOS=WIN95 nodebug=1
		-$(MAKE) -i -f mayu-bcc.mak TARGETOS=WIN95
		cd s
		-$(MAKE) -i -f setup-bcc.mak batch
		cd ..

batch_clean:
		-$(MAKE) -i -f mayu-bcc.mak TARGETOS=WINNT nodebug=1 clean
		-$(MAKE) -i -f mayu-bcc.mak TARGETOS=WINNT clean
		-$(MAKE) -i -f mayu-bcc.mak TARGETOS=WIN95 nodebug=1 clean
		-$(MAKE) -i -f mayu-bcc.mak TARGETOS=WIN95 clean
		cd s
		-$(MAKE) -i -f setup-bcc.mak batch_clean
		cd ..

batch_distclean:	batch_clean
		-$(MAKE) -i -f mayu-bcc.mak TARGETOS=WINNT distclean

batch_distrib: batch
		-$(MAKE) -i -f mayu-bcc.mak TARGETOS=WINNT nodebug=1 distrib
		-$(MAKE) -i -f mayu-bcc.mak TARGETOS=WIN95 nodebug=1 distrib
