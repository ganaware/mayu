############################################################## -*- Makefile -*-
#
# Makefile for mayu
#
###############################################################################


VERSION		= 3.19

!if "$(TARGETOS)" == "WINNT"
OS_SPECIFIC_DEFINES	=  -DUNICODE -D_UNICODE
DISTRIB_OS	= nt
!endif
!if "$(TARGETOS)" == "WIN95"
OS_SPECIFIC_DEFINES	=  -D_MBCS
DISTRIB_OS	= 9x
!endif
!if "$(TARGETOS)" == "BOTH"
!error Must specify TARGETOS=WIN95 or TARGETOS=WINNT
!endif


COMMON_DEFINES	= -DSTRICT -D_WIN32_IE=0x0500 $(OS_SPECIFIC_DEFINES)
BOOST_DIR	= ../boost


# mayu.exe	###############################################################

TARGET_1	= mayu.exe
OBJS_1		=			\
		dlgeditsetting.obj	\
		dlginvestigate.obj	\
		dlglog.obj		\
		dlgsetting.obj		\
		dlgversion.obj		\
		engine.obj		\
		focus.obj		\
		function.obj		\
		keyboard.obj		\
		keymap.obj		\
		layoutmanager.obj	\
		mayu.obj		\
		parser.obj		\
		registry.obj		\
		setting.obj		\
		stringtool.obj		\
		target.obj		\
		vkeytable.obj		\
		windowstool.obj		\

SRCS_1		=			\
		dlgeditsetting.cpp	\
		dlginvestigate.cpp	\
		dlglog.cpp		\
		dlgsetting.cpp		\
		dlgversion.cpp		\
		engine.cpp		\
		focus.cpp		\
		function.cpp		\
		keyboard.cpp		\
		keymap.cpp		\
		layoutmanager.cpp	\
		mayu.cpp		\
		parser.cpp		\
		registry.cpp		\
		setting.cpp		\
		stringtool.cpp		\
		target.cpp		\
		vkeytable.cpp		\
		windowstool.cpp		\

RES_1		= mayu.res

LIBS_1		=		\
		$(guixlibsmt)	\
		shell32.lib	\
		shlwapi.lib	\
		comctl32.lib	\
		mayu.lib	\

EXTRADEP_1	= mayu.lib

# mayu.dll	###############################################################

TARGET_2	= mayu.dll
OBJS_2		= hook.obj stringtool.obj
SRCS_2		= hook.cpp stringtool.cpp
LIBS_2		= $(guixlibsmt) imm32.lib


# mayu.lib	###############################################################

TARGET_3	= mayu.lib
DLL_3		= mayu.dll


# distribution	###############################################################

DISTRIB_SETTINGS =			\
		104.mayu		\
		104on109.mayu		\
		109.mayu		\
		109on104.mayu		\
		default.mayu		\
		emacsedit.mayu		\
		dot.mayu		\

DISTRIB_MANUAL	=			\
		README.html		\
		CONTENTS.html		\
		CUSTOMIZE.html		\
		MANUAL.html		\
		mayu-banner.png		\
		README.css		\
		syntax.txt		\
		mayu-mode.el		\

DISTRIB_CONTRIBS =				\
		contrib\mayu-settings.txt	\
		contrib\dvorak.mayu		\
		contrib\keitai.mayu		\
		contrib\ax.mayu			\

!if "$(TARGETOS)" == "WINNT"
DISTRIB_DRIVER	= mayud.sys mayudnt4.sys
!endif
!if "$(TARGETOS)" == "WIN95"
DISTRIB_DRIVER	= mayud.vxd
!endif

DISTRIB		=			\
		$(TARGET_1)		\
		$(TARGET_2)		\
		setup.exe		\
		$(DISTRIB_SETTINGS)	\
		$(DISTRIB_MANUAL)	\
		$(DISTRIB_CONTRIBS)	\
		$(DISTRIB_DRIVER)	\

DISTRIBSRC	=			\
		Makefile		\
		*.mak			\
		*.cpp			\
		*.h			\
		*.rc			\
		doc++-header.html	\
		doc++.conf		\
					\
		s\Makefile		\
		s\*.mak			\
		s\*.cpp			\
		s\*.h			\
		s\*.rc			\
		s\*.lib 		\
					\
		r\*			\
					\
		d\Makefile		\
		d\SOURCES		\
		d\*.c			\
		d\README.txt		\
		d\test.reg 		\
		d\nt4\Makefile		\
		d\nt4\SOURCES		\
		d\nt4\*.c		\
					\
		d_win9x\Makefile	\
		d_win9x\mayud.asm	\
		d_win9x\mayud.def	\
		d_win9x\mayud.vxd	\
					\
		tools\makedepend	\
		tools\dos2unix		\
		tools\unix2dos		\


# tools		###############################################################

CAB		= s\cab32.exe
DOCXX		= doc++.exe
MAKEDEPEND	= perl tools/makedepend -o.obj
DOS2UNIX	= perl tools/dos2unix
UNIX2DOS	= perl tools/unix2dos
MAKEFUNC	= perl tools/makefunc

# rules		###############################################################

all:		boost $(TARGET_1) $(TARGET_2) $(TARGET_3)

functions.h:	engine.h tools/makefunc
		$(MAKEFUNC) < engine.h > functions.h

clean:
		-$(RM) *.obj
		-$(RM) $(TARGET_1) $(TARGET_2) $(TARGET_3)
		-$(RM) *.res *.exp
		-$(RM) mayu.aps mayu.opt *.pdb
		-$(RM) *~ $(CLEAN)

depend::
		$(MAKEDEPEND) -fmayu-common.mak \
		-- $(DEPENDFLAGS) -- $(SRCS_1) $(SRCS_2)

distrib:
		@$(ECHO) "============================================================================="
		@$(ECHO) "before build package:                                                        "
		@$(ECHO) "  (n)make -f ... clean all nodebug=1                                         "
		@$(ECHO) "  cd s; (n)make -f ... clean all nodebug=1                                   "
		@$(ECHO) "  cd d; build                                                                "
		@$(ECHO) "  cd d/nt4; build                                                            "
		@$(ECHO) "============================================================================="
		-@$(RM) source.cab
		-@$(RM) mayu-$(VERSION)-$(DISTRIB_OS).cab
		-@$(RM) mayu-$(VERSION)-$(DISTRIB_OS).exe
		@$(CAB) -a source.cab -ml:21 $(DISTRIBSRC)
		@$(COPY) d\i386\mayud.sys .
		@$(COPY) d\nt4\i386\mayudnt4.sys .
		@$(COPY) d_win9x\mayud.vxd .
		@$(COPY) s\setup.exe setup.exe
		@$(ECHO) "============================================================================="
		@$(ECHO) "   解凍時のタイトル(T):                   なし                               "
		@$(ECHO) "   解凍メッセージ(M):                     なし                               "
		@$(ECHO) "   標準の解凍先フォルダ(P):               ％temp％ (％ は %% でね)            "
		@$(ECHO) "レ 解凍時に解凍先フォルダを問い合わせない                                    "
		@$(ECHO) "   解凍後、実行または開くファイル名(C):   setup.exe -s                       "
		@$(ECHO) "レ プログラム終了後、解凍されたファイルを削除する                            "
		@$(ECHO) "============================================================================="
		$(UNIX2DOS) $(DISTRIB_SETTINGS) $(DISTRIB_CONTRIBS)
		$(CAB) -a mayu-$(VERSION)-$(DISTRIB_OS).cab -ml:21 $(DISTRIB) source.cab
		$(DOS2UNIX) $(DISTRIB_SETTINGS) $(DISTRIB_CONTRIBS)
		-@$(RM) source.cab mayud.sys mayudnt4.sys mayud.vxd setup.exe
		$(CAB) -f mayu-$(VERSION)-$(DISTRIB_OS).cab
		-@$(RM) mayu-$(VERSION)-$(DISTRIB_OS).cab

srcdesc::
		@$(ECHO) USE DOC++ 3.4.4 OR HIGHER
		$(DOCXX) *.h

# DO NOT DELETE

dlgeditsetting.obj: compiler_specific.h dlgeditsetting.h layoutmanager.h \
 mayurc.h misc.h stringtool.h windowstool.h
dlginvestigate.obj: compiler_specific.h dlginvestigate.h driver.h engine.h \
 focus.h function.h functions.h hook.h keyboard.h keymap.h mayurc.h misc.h \
 msgstream.h multithread.h parser.h setting.h stringtool.h target.h \
 vkeytable.h windowstool.h
dlglog.obj: compiler_specific.h layoutmanager.h mayu.h mayurc.h misc.h \
 msgstream.h multithread.h registry.h stringtool.h windowstool.h
dlgsetting.obj: compiler_specific.h dlgeditsetting.h driver.h function.h \
 functions.h keyboard.h keymap.h layoutmanager.h mayu.h mayurc.h misc.h \
 multithread.h parser.h registry.h setting.h stringtool.h windowstool.h
dlgversion.obj: compiler_specific.h mayu.h mayurc.h misc.h stringtool.h \
 windowstool.h
engine.obj: compiler_specific.h driver.h engine.h errormessage.h function.h \
 functions.h hook.h keyboard.h keymap.h mayurc.h misc.h msgstream.h \
 multithread.h parser.h setting.h stringtool.h windowstool.h
focus.obj: compiler_specific.h focus.h misc.h stringtool.h windowstool.h
function.obj: compiler_specific.h driver.h engine.h function.h functions.h \
 hook.h keyboard.h keymap.h misc.h msgstream.h multithread.h parser.h \
 setting.h stringtool.h vkeytable.h windowstool.h
keyboard.obj: compiler_specific.h driver.h keyboard.h misc.h stringtool.h
keymap.obj: compiler_specific.h driver.h errormessage.h function.h \
 keyboard.h keymap.h misc.h stringtool.h
layoutmanager.obj: compiler_specific.h layoutmanager.h misc.h stringtool.h \
 windowstool.h
mayu.obj: compiler_specific.h dlginvestigate.h dlglog.h dlgsetting.h \
 dlgversion.h driver.h engine.h errormessage.h focus.h function.h \
 functions.h hook.h keyboard.h keymap.h mayu.h mayurc.h misc.h msgstream.h \
 multithread.h parser.h registry.h setting.h stringtool.h target.h \
 windowstool.h
parser.obj: compiler_specific.h errormessage.h misc.h parser.h stringtool.h
registry.obj: compiler_specific.h misc.h registry.h stringtool.h
setting.obj: compiler_specific.h dlgsetting.h driver.h errormessage.h \
 function.h functions.h keyboard.h keymap.h mayu.h mayurc.h misc.h \
 multithread.h parser.h registry.h setting.h stringtool.h vkeytable.h \
 windowstool.h
stringtool.obj: compiler_specific.h misc.h stringtool.h
target.obj: compiler_specific.h mayurc.h misc.h stringtool.h target.h \
 windowstool.h
vkeytable.obj: compiler_specific.h misc.h vkeytable.h
windowstool.obj: compiler_specific.h misc.h stringtool.h windowstool.h
hook.obj: compiler_specific.h hook.h misc.h stringtool.h
stringtool.obj: compiler_specific.h misc.h stringtool.h
