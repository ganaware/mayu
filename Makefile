############################################################## -*- Makefile -*-
#
# Makefile for mayu
#
###############################################################################

F	=	mayu-vc.mak

all:
	@echo "============================================================================="
	@echo "Visual C++ 6.0: nmake"
	@echo "Borland C++ 5.5: make F=mayu-bcc.mak (NOT IMPREMENTED YET)"
	@echo "============================================================================="
	$(MAKE) -f $(F) $(MAKEFLAGS) batch

clean:
	$(MAKE) -f $(F) $(MAKEFLAGS) batch_clean

distrib:
	$(MAKE) -f $(F) $(MAKEFLAGS) batch_distrib

depend:
	$(MAKE) -f $(F) $(MAKEFLAGS) TARGETOS=WINNT depend
