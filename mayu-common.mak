############################################################## -*- Makefile -*-
#
# Makefile for mayu
#
###############################################################################


VERSION		= 3.17

COMMON_DEFINES	= -DSTRICT -D_WIN32_IE=0x0400


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
		mayu.obj		\
		msgstream.obj		\
		parser.obj		\
		regexp.obj		\
		registry.obj		\
		setting.obj		\
		stringtool.obj		\
		target.obj		\
		vkeytable.obj		\
		windowstool.obj		\

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
OBJS_2		= hook.obj
LIBS_2		= $(guixlibsmt) imm32.lib


# mayu.lib	###############################################################

TARGET_3	= mayu.lib
DLL_3		= mayu.dll


# distribution	###############################################################

DISTRIB_SETTINGS =						\
					en\104.mayu		\
		ja\104on109.mayu	en\104on109.mayu	\
		ja\109.mayu		en\109.mayu		\
					en\109on104.mayu	\
		ja\default.mayu		en\default.mayu		\
		ja\emacsedit.mayu	en\emacsedit.mayu	\
		ja\dot.mayu		en\dot.mayu		\

DISTRIB_MANUAL	=						\
		ja\README.html		en\README.html		\
		ja\CONTENTS.html	en\CONTENTS.html	\
		ja\CUSTOMIZE.html	en\CUSTOMIZE.html	\
		ja\MANUAL.html		en\MANUAL.html		\
		ja\README.css		en\README.css		\
					en\syntax.txt		\

DISTRIB_CONTRIBS =				\
		contrib\mayu-settings.txt	\
		contrib\dvorak.mayu		\

DISTRIB		=			\
		$(TARGET_1)		\
		$(TARGET_2)		\
		mayud.sys		\
		mayudnt4.sys		\
		setup.exe		\
		$(DISTRIB_SETTINGS)	\
		$(DISTRIB_MANUAL)	\
		$(DISTRIB_CONTRIBS)	\

DISTRIBSRC	=			\
		Makefile		\
		*.mak			\
		*.cpp			\
		*.h			\
		*.rc			\
		regexp.html		\
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


# tools		###############################################################

CAB		= s\cab32.exe
DOCXX		= doc++.exe


# rules		###############################################################

all:		$(TARGET_1) $(TARGET_2) $(TARGET_3)

clean:
		-$(RM) *.obj
		-$(RM) $(TARGET_1) $(TARGET_2) $(TARGET_3)
		-$(RM) *.res *.exp
		-$(RM) *~ $(CLEAN)

distrib:
		@$(ECHO) "============================================================================="
		@$(ECHO) "before build package:                                                        "
		@$(ECHO) "  (n)make -f ... clean all nodebug=1                                         "
		@$(ECHO) "  cd s; (n)make -f ... clean all nodebug=1                                   "
		@$(ECHO) "  cd d; build                                                                "
		@$(ECHO) "  cd d/nt4; build                                                            "
		@$(ECHO) "============================================================================="
		-@$(RM) source.cab
		-@$(RM) mayu-$(VERSION).cab
		-@$(RM) mayu-$(VERSION).exe
		@$(CAB) -a source.cab -ml:21 $(DISTRIBSRC)
		@$(COPY) d\i386\mayud.sys .
		@$(COPY) d\nt4\i386\mayudnt4.sys .
		@$(COPY) s\setup.exe setup.exe
		@$(ECHO) "============================================================================="
		@$(ECHO) "   解凍時のタイトル(T):                   なし                               "
		@$(ECHO) "   解凍メッセージ(M):                     なし                               "
		@$(ECHO) "   標準の解凍先フォルダ(P):               ％temp％ (％ は %% でね)            "
		@$(ECHO) "レ 解凍時に解凍先フォルダを問い合わせない                                    "
		@$(ECHO) "   解凍後、実行または開くファイル名(C):   setup.exe -s                       "
		@$(ECHO) "レ プログラム終了後、解凍されたファイルを削除する                            "
		@$(ECHO) "============================================================================="
		$(CAB) -a mayu-$(VERSION).cab -ml:21 $(DISTRIB) source.cab
		-@$(RM) source.cab mayud.sys mayudnt4.sys setup.exe
		$(CAB) -f mayu-$(VERSION).cab
		-@$(RM) mayu-$(VERSION).cab

srcdesc::
		@$(ECHO) USE DOC++ 3.4.4-pre5 OR HIGHER
		$(DOCXX) *.h *.cpp
