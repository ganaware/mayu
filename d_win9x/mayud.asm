PAGE 58,132
   .386p

    .xlist
    include vmm.inc
    include debug.inc
	include	vwin32.inc
	include vkd.inc
    .list

; the following equate makes the VXD dynamically loadable.
MAYUD_DYNAMIC EQU 1

MAYUD_DEVICE_ID EQU 18ABH
FlagBreak equ 10000h
FlagE0 equ 20000h
FlagE1 equ 40000h

PreCodeE0 equ 0e0h
PreCodeE1 equ 0e1h

BreakMask equ 80h
CodeMask equ 7fh

VXD_LOCKED_DATA_SEG

Keyboard_Proc	dd	0
FlagsBuf		dd	0
readp			dd	0
writep			dd	0
CodeBuf			dd	16	dup (0)
ForceBuf		db	3		dup (0)
BlockingID		dd	0

VXD_LOCKED_DATA_ENDS

DECLARE_VIRTUAL_DEVICE    MAYUD, 1, 0, MAYUD_Control, MAYUD_DEVICE_ID, \
                        UNDEFINED_INIT_ORDER, , 

VxD_LOCKED_CODE_SEG

BeginProc MAYUD_Control
    Control_Dispatch SYS_DYNAMIC_DEVICE_INIT, MAYUD_Dynamic_Init
    Control_Dispatch SYS_DYNAMIC_DEVICE_EXIT, MAYUD_Dynamic_Exit
    Control_Dispatch W32_DEVICEIOCONTROL,     MAYUD_W32_DeviceIOControl
    clc
    ret
EndProc MAYUD_Control

BeginProc MAYUD_W32_DeviceIOControl
	cmp		ecx, DIOC_CLOSEHANDLE
	je		DevCtrl_Success
	cmp		ecx, DIOC_OPEN
	je		DevCtrl_Success
	cmp		ecx, 1
	je		ReadFromBuffer
WriteToVKD:
	mov		eax, [esi].lpvInBuffer
	mov		esi, offset32 ForceBuf
	mov		ecx, esi
	add		eax, 2h
	mov		ebx, [eax]
ForceE0:
	test	ebx, FlagE0
	jz		ForceE1
	mov		byte ptr [ecx], PreCodeE0
	inc		ecx
ForceE1:
	test	ebx, FlagE1
	jz		ForceBreak
	mov		byte ptr [ecx], PreCodeE1
	inc		ecx
ForceBreak:
	test	ebx, FlagBreak
	jz		ForceKey
	or		bl, 80h
ForceKey:
	mov		byte ptr [ecx], bl
	inc		ecx
	sub		ecx, esi
	VxDcall	VKD_Force_Keys
	jmp		DevCtrl_Success

ReadFromBuffer:
	mov		ecx, readp
	cmp		ecx, writep
	je		BufferUnderRun
	mov		eax, [esi].lpvOutBuffer
	add		eax, 2h
	mov		ebx, [ecx]
	mov		[eax], ebx
	add		ecx, 4h
	mov		readp, ecx
	sub		ecx, offset CodeBuf
	cmp		ecx, 14h
	jne		DevCtrl_Success
	mov		readp, offset CodeBuf
DevCtrl_Success:
	xor		eax, eax
	clc
	ret
BufferUnderRun:
	mov		BlockingID, edx
	VMMCall _BlockOnID, <edx, BLOCK_SVC_INTS+BLOCK_ENABLE_INTS>
	mov		BlockingID, 0h
	jmp		ReadFromBuffer
DevCtrl_Fail:
	mov		eax, 1
	stc
	ret

EndProc   MAYUD_W32_DeviceIOControl

BeginProc MAYUD_Dynamic_Exit
	GetVxDServiceOrdinal eax, VKD_Filter_Keyboard_Input
	mov	esi, offset32 Mayud
	VMMCall	Unhook_Device_Service
	clc
	mov		eax, VXD_SUCCESS
    ret
EndProc   MAYUD_Dynamic_Exit

BeginProc MAYUD_Dynamic_Init
	GetVxDServiceOrdinal eax, VKD_Filter_Keyboard_Input
	mov	esi, offset32 Mayud
	VMMCall	Hook_Device_Service
	mov	Keyboard_Proc, esi
	mov	eax, offset32 CodeBuf
	mov readp, eax
	mov	writep, eax
	clc
	mov		eax, VXD_SUCCESS
    ret
EndProc   MAYUD_Dynamic_Init

VxD_LOCKED_CODE_ENDS

VXD_PAGEABLE_CODE_SEG

BeginProc Mayud, Hook_Proc Keyboard_Proc
CheckE0:
	cmp		cl, PreCodeE0
	jnz		CheckE1	
	or		FlagsBuf, FlagE0
	jmp		EatTheCode
CheckE1:
	cmp		cl, PreCodeE1
	jnz		CheckBreak
	or		FlagsBuf, FlagE1
	jmp		EatTheCode
CheckBreak:
	test	cl, BreakMask
	jz		SaveScanCode
	or		FlagsBuf, FlagBreak
SaveScanCode:
	and		ecx, CodeMask
	or		ecx, FlagsBuf
	mov		eax, writep
	mov		[eax], ecx
	add		eax, 4h
	mov		writep, eax
	mov		FlagsBuf, 0h
	sub		eax, offset CodeBuf
	cmp		eax, 14h
	jne		UnBlockReading
	mov		writep, offset CodeBuf
UnBlockReading:
	mov		eax, BlockingID
	cmp		eax, 0h
	je		EatTheCode
	VMMCall _SignalID <eax>
EatTheCode:
	mov		ecx, 0h
	clc
	ret
EndProc Mayud

VXD_PAGEABLE_CODE_ENDS

end
