############################################################## -*- Makefile -*-
#
# Makefile for setup (Visual C++ 6.0)
#
#	make release version: nmake nodebug=1
#	make debug version: nmake
#
###############################################################################


INCLUDES	= -I$(BOOST_DIR)	# why here ?
DEPENDIGNORE	= --ignore=$(BOOST_DIR)

!include <..\vc.mak>
!include <setup-common.mak>

LDFLAGS_1	=						\
		$(guilflags)					\
		/PDB:$(TARGET_1).pdb				\
		/LIBPATH:$(BOOST_DIR)/libs/regex/lib/vc6	\

LDFLAGS_2	= $(conlflags)

$(TARGET_1):	$(OBJS_1) $(RES_1) $(EXTRADEP_1)
	$(link) -out:$@ $(ldebug) $(LDFLAGS_1) $(OBJS_1) $(LIBS_1) $(RES_1)

strres.h:	setup.rc
	grep IDS setup.rc | \
	sed "s/\(IDS[a-zA-Z0-9_]*\)[^""]*\("".*\)$$/\1, _T(\2),/" | \
	sed "s/""""/"") _T(""/g" > strres.h

batch:
		-$(MAKE) -k -f setup-vc.mak TARGETOS=WINNT nodebug=1
		-$(MAKE) -k -f setup-vc.mak TARGETOS=WINNT
		-$(MAKE) -k -f setup-vc.mak TARGETOS=WIN95 nodebug=1
		-$(MAKE) -k -f setup-vc.mak TARGETOS=WIN95

batch_clean:
		-$(MAKE) -k -f setup-vc.mak TARGETOS=WINNT nodebug=1 clean
		-$(MAKE) -k -f setup-vc.mak TARGETOS=WINNT clean
		-$(MAKE) -k -f setup-vc.mak TARGETOS=WIN95 nodebug=1 clean
		-$(MAKE) -k -f setup-vc.mak TARGETOS=WIN95 clean
