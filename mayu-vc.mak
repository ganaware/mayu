############################################################## -*- Makefile -*-
#
# Makefile for mayu (Visual C++ 6.0)
#
#	make release version: nmake nodebug=1
#	make debug version: nmake
#
###############################################################################

INCLUDES	= -I$(BOOST_DIR)	# why here ?
DEPENDIGNORE	= --ignore=$(BOOST_DIR)

!include <vc.mak>
!include <mayu-common.mak>

DEFINES		= $(COMMON_DEFINES) -DVERSION=""""$(VERSION)""""
# INCLUDES	= -I$(BOOST_DIR)	# make -f mayu-vc.mak depend fails ...

LDFLAGS_1	=						\
		$(guilflags)					\
		/PDB:$(TARGET_1).pdb				\
		/LIBPATH:$(BOOST_DIR)/libs/regex/lib/vc6	\

LDFLAGS_2	=						\
		$(dlllflags)					\
		/PDB:$(TARGET_2).pdb				\
		/LIBPATH:$(BOOST_DIR)/libs/regex/lib/vc6	\

$(TARGET_1):	$(OBJS_1) $(RES_1) $(EXTRADEP_1)
	$(link) -out:$@ $(ldebug) $(LDFLAGS_1) $(OBJS_1) $(LIBS_1) $(RES_1)

$(TARGET_2):	$(OBJS_2) $(RES_2) $(EXTRADEP_2)
	$(link) -out:$@ $(ldebug) $(LDFLAGS_2) $(OBJS_2) $(LIBS_2) $(RES_2)

$(TARGET_3):	$(DLL_3)

REGEXPP_XCFLAGS	= $(REGEXPP_XCFLAGS) XCFLAGS=-D_WCTYPE_INLINE_DEFINED

boost:
		cd $(BOOST_DIR)/libs/regex/lib/
		$(MAKE) -f vc6.mak $(REGEXPP_XCFLAGS)
		cd ../../../../mayu

distclean::	clean
		cd $(BOOST_DIR)/libs/regex/lib/
		-$(MAKE) -k -f vc6.mak clean
		cd ../../../../mayu

batch:
		-$(MAKE) -k -f mayu-vc.mak TARGETOS=WINNT nodebug=1
		-$(MAKE) -k -f mayu-vc.mak TARGETOS=WINNT
		-$(MAKE) -k -f mayu-vc.mak TARGETOS=WIN95 nodebug=1
		-$(MAKE) -k -f mayu-vc.mak TARGETOS=WIN95
		cd s
		-$(MAKE) -k -f setup-vc.mak batch
		cd ..

batch_clean:
		-$(MAKE) -k -f mayu-vc.mak TARGETOS=WINNT nodebug=1 clean
		-$(MAKE) -k -f mayu-vc.mak TARGETOS=WINNT clean
		-$(MAKE) -k -f mayu-vc.mak TARGETOS=WIN95 nodebug=1 clean
		-$(MAKE) -k -f mayu-vc.mak TARGETOS=WIN95 clean
		cd s
		-$(MAKE) -k -f setup-vc.mak batch_clean
		cd ..

batch_distrib: batch
		-$(MAKE) -k -f mayu-vc.mak TARGETOS=WINNT distrib
		-$(MAKE) -k -f mayu-vc.mak TARGETOS=WIN95 distrib
