############################################################## -*- Makefile -*-
#
# Makefile for mayu
#
###############################################################################

F	=	mayu-vc.mak

all:
	@echo "============================================================================="
	@echo "Visual C++ 6.0:"
	@echo "        nmake"
	@echo "        nmake clean"
	@echo "        nmake distrib"
	@echo "        nmake depend"
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
