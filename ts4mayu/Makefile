#
# Makefile for VC
#

!include <win32.mak>

SRCS = ts4mayu.cpp
HEADERS = 
OBJS = ts4mayu.obj
DEFS = ts4mayu.def
TARGET_NAME = ts4mayu
TARGET = $(TARGET_NAME).dll

CFLAGS = -DUNICODE -D_UNICODE -D_MT -MT
INCLUDES = -I../../SynCOMAPIv1_0/Include
LDFLAGS =  $(dlllflags) /libpath:../../SynCOMAPIv1_0/Lib
LDLIBS =  SynCom.lib
#LDLIBS =  $(guilibsmt) comctl32.lib psapi.lib shell32.lib

RM = del

all: $(TARGET)

$(TARGET): $(OBJS) $(DEFS)
	$(link) $(LDFLAGS) $(OBJS) -def:$(DEFS) $(LDLIBS) -out:$@

{}.cpp.obj:
	$(cc) -GX $(cdebug) $(cflags) $(cvarsmt) $(DEFINES) $(INCLUDES) \
		$(DEBUG_FLAG) -Fo$@ $(*B).cpp

clean:
	-$(RM) $(TARGET) $(OBJS) *.exp *.lib *~
