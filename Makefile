############################################################## -*- Makefile -*-
#
# Makefile for mayu
#
###############################################################################

all:
	@echo ===============================================================================
	@echo for WindowsNT/2000
	@echo Visual C++ 6.0: nmake -f mayu-vc.mak nodebug=1
	@echo Borland C++ 5.5: make -f mayu-bcc.mak nodebug=1
	@echo for Windows95/98/Me
	@echo Visual C++ 6.0: nmake -f mayu-vc.mak nodebug=1 TARGETOS=WIN95
	@echo Visual C++ 6.0: nmake -f mayu-vc.mak nodebug=1 TARGETOS=WIN95
	@echo Borland C++ 5.5: make -f mayu-bcc.mak nodebug=1 TARGETOS=WIN95
	@echo ===============================================================================
