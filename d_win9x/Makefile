#****************************************************************************
#                                                                           *
# THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY     *
# KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE       *
# IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR     *
# PURPOSE.                                                                  *
#                                                                           *
# Copyright 1993-95  Microsoft Corporation.  All Rights Reserved.           *
#                                                                           *
#****************************************************************************

BUILD_TYPE=base
!INCLUDE $(BASEDIR)\inc\master.mk

LFLAGS     = -machine:i386 -debug:notmapped,full -debugtype:cv -subsystem:console kernel32.lib 
AFLAGS     = -coff -DBLD_COFF -DIS_32 -W2 -Zd -c -Cx -DMASM6 -DDEBLEVEL=1 -DDEBUG -I$(INC32)

all: mayud.vxd

mayud.obj: mayud.asm
        ml $(AFLAGS) -Fo$@ %s

mayud.vxd: mayud.obj mayud.def
        echo >NUL @<<mayud.crf
-MACHINE:i386 -DEBUG:NONE -PDB:NONE -ignore:4078 -ignore:4039
-DEF:mayud.def -OUT:mayud.vxd -MAP:mayud.map -LIBPATH:$(LIB32)
-VXD $(BASEDIR)\lib\i386\free\vxdwraps.clb mayud.obj
<<
!ifndef PASS0ONLY
		@echo link -OUT:$@
        link @mayud.crf
        mapsym mayud
!endif

clean:
        -@if exist *.obj del *.obj
        -@if exist *.vxd del *.vxd
        -@if exist *.exp del *.exp
        -@if exist *.map del *.map
        -@if exist *.sym del *.sym
        -@if exist *.lib del *.lib
        -@if exist *.pdb del *.pdb
        -@if exist *.bak del *.bak
