// ////////////////////////////////////////////////////////////////////////////
// dlginvestigate.h

#ifndef __dlginvestigate_h__
#define __dlginvestigate_h__


#include <windows.h>
#include "stringtool.h"


///
BOOL CALLBACK dlgInvestigate_dlgProc(HWND hwnd, UINT message,
				     WPARAM wParam, LPARAM lParam);

class Engine;

///
class DlgInvestigateData
{
public:
  Engine *m_engine;
  HWND m_hwndLog;
};

#endif __dlginvestigate_h__
