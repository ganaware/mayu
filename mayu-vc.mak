############################################################## -*- Makefile -*-
#
# Makefile for mayu (Visual C++)
#
#	make release version: nmake nodebug=1
#	make debug version: nmake
#
###############################################################################

INCLUDES	= -I$(BOOST_DIR)	# why here ?
DEPENDIGNORE	= --ignore=$(BOOST_DIR)

!if "$(MAYU_VC)" == ""
MAYU_VC	= vc6
!endif

!if "$(MAYU_VC)" == "vc71"
MAYU_REGEX_VC	= vc7
!else
MAYU_REGEX_VC	= $(MAYU_VC)
!endif

!include <vc.mak>
!include <mayu-common.mak>

DEFINES		= $(COMMON_DEFINES) -DVERSION=""""$(VERSION)"""" \
		-DLOGNAME=""""$(LOGNAME)"""" \
		-DCOMPUTERNAME=""""$(COMPUTERNAME)""""
# INCLUDES	= -I$(BOOST_DIR)	# make -f mayu-vc.mak depend fails ...

LDFLAGS_1	=						\
		$(guilflags)					\
		/PDB:$(TARGET_1).pdb				\
		/LIBPATH:$(BOOST_DIR)/libs/regex/build/$(MAYU_REGEX_VC)	\

LDFLAGS_2	=						\
		$(dlllflags)					\
		/PDB:$(TARGET_2).pdb				\
		/LIBPATH:$(BOOST_DIR)/libs/regex/build/$(MAYU_REGEX_VC)	\

$(TARGET_1):	$(OBJS_1) $(RES_1) $(EXTRADEP_1)
	$(link) -out:$@ $(ldebug) $(LDFLAGS_1) $(OBJS_1) $(LIBS_1) $(RES_1)

$(TARGET_2):	$(OBJS_2) $(RES_2) $(EXTRADEP_2)
	$(link) -out:$@ $(ldebug) $(LDFLAGS_2) $(OBJS_2) $(LIBS_2) $(RES_2)

$(TARGET_3):	$(DLL_3)

REGEXPP_XCFLAGS	= $(REGEXPP_XCFLAGS) XCFLAGS=-D_WCTYPE_INLINE_DEFINED

clean::
		-$(RM) mayu.aps mayu.opt *.pdb

boost:
		cd $(BOOST_DIR)/libs/regex/build/
		$(MAKE) -f $(MAYU_REGEX_VC).mak $(REGEXPP_XCFLAGS) main_dir boost_regex_$(MAYU_REGEX_VC)_mss_dir ./$(MAYU_REGEX_VC)/boost_regex_$(MAYU_REGEX_VC)_mss.lib
		cd ../../../../mayu

distclean::	clean
		cd $(BOOST_DIR)/libs/regex/build/
		-$(MAKE) -k -f $(MAYU_REGEX_VC).mak clean
		cd ../../../../mayu

batch:
		-$(MAKE) -k -f mayu-vc.mak MAYU_VC=$(MAYU_VC) TARGETOS=WINNT nodebug=1
		-$(MAKE) -k -f mayu-vc.mak MAYU_VC=$(MAYU_VC) TARGETOS=WINNT
		-$(MAKE) -k -f mayu-vc.mak MAYU_VC=$(MAYU_VC) TARGETOS=WIN95 nodebug=1
		-$(MAKE) -k -f mayu-vc.mak MAYU_VC=$(MAYU_VC) TARGETOS=WIN95
		cd s
		-$(MAKE) -k -f setup-vc.mak MAYU_VC=$(MAYU_VC) batch
		cd ..

batch_clean:
		-$(MAKE) -k -f mayu-vc.mak MAYU_VC=$(MAYU_VC) TARGETOS=WINNT nodebug=1 clean
		-$(MAKE) -k -f mayu-vc.mak MAYU_VC=$(MAYU_VC) TARGETOS=WINNT clean
		-$(MAKE) -k -f mayu-vc.mak MAYU_VC=$(MAYU_VC) TARGETOS=WIN95 nodebug=1 clean
		-$(MAKE) -k -f mayu-vc.mak MAYU_VC=$(MAYU_VC) TARGETOS=WIN95 clean
		cd s
		-$(MAKE) -k -f setup-vc.mak MAYU_VC=$(MAYU_VC) batch_clean
		cd ..

batch_distclean: batch_clean
		-$(MAKE) -k -f mayu-vc.mak MAYU_VC=$(MAYU_VC) TARGETOS=WINNT distclean

batch_distrib: batch
		-$(MAKE) -k -f mayu-vc.mak MAYU_VC=$(MAYU_VC) TARGETOS=WINNT nodebug=1 distrib
		-$(MAKE) -k -f mayu-vc.mak MAYU_VC=$(MAYU_VC) TARGETOS=WIN95 nodebug=1 distrib
