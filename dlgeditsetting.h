// ////////////////////////////////////////////////////////////////////////////
// dlgeditsetting.h


#ifndef _DLGEDITSETTING_H
#  define _DLGEDITSETTING_H

#  ifndef _STRINGTOOL_H
#    include "stringtool.h"
#  endif // _STRINGTOOL_H


/// dialog procedure of "Edit Setting" dialog box
BOOL CALLBACK dlgEditSetting_dlgProc(
  HWND i_hwnd, UINT i_message, WPARAM i_wParam, LPARAM i_lParam);

/// parameters for "Edit Setting" dialog box
class DlgEditSettingData
{
public:
  StringTool::istring m_name;			/// setting name
  StringTool::istring m_filename;		/// filename of setting
  StringTool::istring m_symbols; /// symbol list (-Dsymbol1;-Dsymbol2;-D...)
};


#endif _DLGEDITSETTING_H
