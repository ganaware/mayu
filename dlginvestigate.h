// ////////////////////////////////////////////////////////////////////////////
// dlginvestigate.h


#ifndef _DLGINVESTIGATE_H
#  define _DLGINVESTIGATE_H

#  ifndef _STRINGTOOL_H
#    include "stringtool.h"
#  endif // _STRINGTOOL_H


/// dialog procedure of "Investigate" dialog box
BOOL CALLBACK dlgInvestigate_dlgProc(
  HWND i_hwnd, UINT i_message, WPARAM i_wParam, LPARAM i_lParam);

class Engine;

/// parameters for "Investigate" dialog box
class DlgInvestigateData
{
public:
  Engine *m_engine;				/// engine
  HWND m_hwndLog;				/// log
};


#endif // _DLGINVESTIGATE_H
