// ////////////////////////////////////////////////////////////////////////////
// dlgeditsetting.h


#ifndef __dlgeditsetting_h__
#define __dlgeditsetting_h__


#include <windows.h>

#include "stringtool.h"


///
BOOL CALLBACK dlgEditSetting_dlgProc(HWND hwnd, UINT message,
				     WPARAM wParam, LPARAM lParam);

///
class DlgEditSettingData
{
public:
  StringTool::istring name;	///
  StringTool::istring filename;	///
  StringTool::istring symbols;	///
};


#endif __dlgeditsetting_h__
