############################################################## -*- Makefile -*-
#
# Makefile for mayu
#
###############################################################################

F	=	mayu-vc.mak

all:
	@echo "============================================================================="
	@echo "Visual C++ 6.0:"
	@echo "        nmake MAYU_VC=vc6"
	@echo "        nmake MAYU_VC=vc6 clean"
	@echo "        nmake MAYU_VC=vc6 distrib"
	@echo "        nmake MAYU_VC=vc6 depend"
	@echo "Visual C++ 7.0:"
	@echo "        nmake MAYU_VC=vc7"
	@echo "        nmake MAYU_VC=vc7 clean"
	@echo "        nmake MAYU_VC=vc7 distrib"
	@echo "        nmake MAYU_VC=vc7 depend"
	@echo "Visual C++ 7.1:"
	@echo "        nmake MAYU_VC=vc71"
	@echo "        nmake MAYU_VC=vc71 clean"
	@echo "        nmake MAYU_VC=vc71 distrib"
	@echo "        nmake MAYU_VC=vc71 depend"
	@echo "Borland C++ 5.5:"
	@echo "        make -f mayu-bcc.mak batch"
	@echo "        make -f mayu-bcc.mak batch_clean"
	@echo "        make -f mayu-bcc.mak batch_distrib"
	@echo "============================================================================="
	$(MAKE) -f $(F) $(MAKEFLAGS) batch

clean:
	$(MAKE) -f $(F) $(MAKEFLAGS) batch_clean

distclean:
	$(MAKE) -f $(F) $(MAKEFLAGS) batch_distclean

distrib:
	$(MAKE) -f $(F) $(MAKEFLAGS) batch_distrib

depend:
	$(MAKE) -f $(F) $(MAKEFLAGS) TARGETOS=WINNT depend
