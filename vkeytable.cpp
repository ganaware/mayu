///////////////////////////////////////////////////////////////////////////////
// vkeytable.cpp


#include "vkeytable.h"

#include <windows.h>
#include <ime.h>


#define VK(name) { VK_##name, #name }

const VKeyTable vkeyTable[] =
{
  VK(LBUTTON),				// 01 LButton
  VK(RBUTTON),				// 02 RButton
  VK(CANCEL),				// 03 Break
  VK(MBUTTON),				// 04 MButton
  VK(BACK),				// 08 BackSpace
  VK(TAB),				// 09 Tab
  VK(CLEAR),				// 0C
  VK(RETURN),				// 0D Enter
  VK(SHIFT),				// 10 Shift
  VK(CONTROL),				// 11 Ctrl
  VK(MENU),				// 12 Alt
  VK(PAUSE),				// 13 Pause
  VK(CAPITAL),				// 14 CapsLock
  VK(KANA),				// 15 Eisuu
  VK(HANGUL),				// 15
  VK(HANGEUL),				// 15
  VK(JUNJA),            	      	// 17
  VK(FINAL),            	       	// 18
  VK(KANJI),				// 19 Kanji
  VK(HANJA),            	      	// 19
  VK(ESCAPE),				// 1B Esc
  VK(CONVERT),				// 1C Henkan
  VK(NONCONVERT),			// 1D Muhenkan
  VK(ACCEPT),				// 1E
  VK(MODECHANGE),			// 1F
  VK(SPACE),				// 20 Space
  VK(PRIOR),				// 21 Page Up
  VK(NEXT),				// 22 Page Down
  VK(END),				// 23 End
  VK(HOME),				// 24 Home
  VK(LEFT),				// 25 Left
  VK(UP),				// 26 Up
  VK(RIGHT),				// 27 Right
  VK(DOWN),				// 28 Down
  VK(SELECT),				// 29
  VK(PRINT),				// 2A
  VK(EXECUTE),				// 2B
  VK(SNAPSHOT),				// 2C Print Screen
  VK(INSERT),				// 2D Insert
  VK(DELETE),				// 2E Delete
  { '0', "_0" },			// 30 0
  { '1', "_1" },			// 31 1
  { '2', "_2" },			// 32 2
  { '3', "_3" },			// 33 3
  { '4', "_4" },			// 34 4
  { '5', "_5" },			// 35 5
  { '6', "_6" },			// 36 6
  { '7', "_7" },			// 37 7
  { '8', "_8" },			// 38 8
  { '9', "_9" },			// 39 9
  { 'A', "A" },				// 41 A
  { 'B', "B" },				// 42 B
  { 'C', "C" },				// 43 C
  { 'D', "D" },				// 44 D
  { 'E', "E" },				// 45 E
  { 'F', "F" },				// 46 F
  { 'G', "G" },				// 47 G
  { 'H', "H" },				// 48 H
  { 'I', "I" },				// 49 I
  { 'J', "J" },				// 4A J
  { 'K', "K" },				// 4B K
  { 'L', "L" },				// 4C L
  { 'M', "M" },				// 4D M
  { 'N', "N" },				// 4E N
  { 'O', "O" },				// 4F O
  { 'P', "P" },				// 50 P
  { 'Q', "Q" },				// 51 Q
  { 'R', "R" },				// 52 R
  { 'S', "S" },				// 53 S
  { 'T', "T" },				// 54 T
  { 'U', "U" },				// 55 U
  { 'V', "V" },				// 56 V
  { 'W', "W" },				// 57 W
  { 'X', "X" },				// 58 X
  { 'Y', "Y" },				// 59 Y
  { 'Z', "Z" },				// 5A Z
  VK(LWIN),				// 5B Left Windows
  VK(RWIN),				// 5C Right Windows
  VK(APPS),				// 5D Applications
  VK(NUMPAD0),				// 60 0 (Numpad)
  VK(NUMPAD1),				// 61 1 (Numpad)
  VK(NUMPAD2),				// 62 2 (Numpad)
  VK(NUMPAD3),				// 63 3 (Numpad)
  VK(NUMPAD4),				// 64 4 (Numpad)
  VK(NUMPAD5),				// 65 5 (Numpad)
  VK(NUMPAD6),				// 66 6 (Numpad)
  VK(NUMPAD7),				// 67 7 (Numpad)
  VK(NUMPAD8),				// 68 8 (Numpad)
  VK(NUMPAD9),				// 69 9 (Numpad)
  VK(MULTIPLY),				// 6A * (Numpad)
  VK(ADD),				// 6B + (Numpad)
  VK(SEPARATOR),			// 6C ??
  VK(SUBTRACT),				// 6D - (Numpad)
  VK(DECIMAL),				// 6E . (Numpad)
  VK(DIVIDE),				// 6F / (Numpad)
  VK(F1),		                // 70 F1
  VK(F2),		                // 71 F2
  VK(F3),		                // 72 F3
  VK(F4),		                // 73 F4
  VK(F5),		                // 74 F5
  VK(F6),		                // 75 F6
  VK(F7),		                // 76 F7
  VK(F8),		                // 77 F8
  VK(F9),		                // 78 F9
  VK(F10),		                // 79 F10
  VK(F11),		                // 7A F11
  VK(F12),		                // 7B F12
  VK(F13),		                // 7C F13
  VK(F14),		                // 7D F14
  VK(F15),		                // 7E F15
  VK(F16),		                // 7F F16
  VK(F17),		                // 80 F17
  VK(F18),		                // 81 F18
  VK(F19),		                // 82 F19
  VK(F20),		                // 83 F20
  VK(F21),		                // 84 F21
  VK(F22),		                // 85 F22
  VK(F23),		                // 86 F23
  VK(F24),		                // 87 F24
  VK(NUMLOCK),				// 90 NumLock
  VK(SCROLL),				// 91 Scroll Lock
  VK(PROCESSKEY),			// E5
  VK(DBE_ALPHANUMERIC),			// F0 Eisuu
  VK(DBE_KATAKANA),			// F1 Katakana
  VK(DBE_HIRAGANA),			// F2 Hiragana
  VK(DBE_SBCSCHAR),			// F3 Hankaku
  VK(DBE_DBCSCHAR),			// F4 Zenkaku
  VK(DBE_ROMAN),			// F5 Roman
  VK(DBE_NOROMAN),			// F6 NoRoman
  VK(ATTN),				// F6
  VK(DBE_ENTERWORDREGISTERMODE),	// F7
  VK(CRSEL),				// F7
  VK(DBE_ENTERIMECONFIGMODE),		// F8
  VK(EXSEL),				// F8
  VK(DBE_FLUSHSTRING),			// F9
  VK(EREOF),				// F9
  VK(DBE_CODEINPUT),			// FA
  VK(PLAY),				// FA
  VK(DBE_NOCODEINPUT),			// FB
  VK(ZOOM),				// FB
  VK(DBE_DETERMINESTRING),		// FC
  VK(NONAME),				// FC
  VK(DBE_ENTERDLGCONVERSIONMODE),	// FD
  VK(PA1),				// FD
  { 0, NULL },
};
