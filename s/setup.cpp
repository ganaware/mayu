///////////////////////////////////////////////////////////////////////////////
// setup.cpp


#include "../misc.h"
#include "../registry.h"
#include "../stringtool.h"
#include "../windowstool.h"
#include "../mayu.h"
#include "setuprc.h"
#include "installer.h"

#include <windowsx.h>
#include <shlobj.h>


using namespace Installer;


///////////////////////////////////////////////////////////////////////////////
// Registry


#define DIR_REGISTRY_ROOT			\
	HKEY_LOCAL_MACHINE,			\
	_T("Software\\GANAware\\mayu")


///////////////////////////////////////////////////////////////////////////////
// Globals


enum
{
  Flag_UsbTest = 1 << 1,
};
u_int32 g_flags = SetupFile::Normal;


using namespace SetupFile;
const SetupFile::Data g_setupFiles[] =
{
  // same name
#define SN(i_kind, i_os, i_from, i_destination)			\
  { i_kind, i_os, Normal|Flag_UsbTest, _T(i_from), i_destination, _T(i_from) }
  // different name
#define DN(i_kind, i_os, i_from, i_destination, i_to)	\
  { i_kind, i_os, Normal|Flag_UsbTest, _T(i_from), i_destination, _T(i_to) }
  
  // executables
  SN(Dll , ALL, "mayu.dll"	     , ToDest),
  SN(File, ALL, "mayu.exe"	     , ToDest),
  SN(File, ALL, "setup.exe"	     , ToDest),
				    
  // drivers
#if defined(_WINNT)
  SN(File, NT , "mayud.sys"	     , ToDest),
  SN(File, NT , "mayudusb.sys"	     , ToDest),
  SN(File, NT , "mayudnt4.sys"	     , ToDest),
  { File, W2k, Normal,       _T("mayud.sys"),    ToDriver, _T("mayud.sys") },
  { File, W2k, Flag_UsbTest, _T("mayudusb.sys"), ToDriver, _T("mayud.sys") },
  DN(File, NT4, "mayudnt4.sys"	     , ToDriver, "mayud.sys"),
#elif defined(_WIN95)
  SN(File, W9x, "mayud.vxd"	     , ToDest),
#else
#  error
#endif

  // setting files		    
  SN(File, ALL, "104.mayu"	     , ToDest),
  SN(File, ALL, "104on109.mayu"	     , ToDest),
  SN(File, ALL, "109.mayu"	     , ToDest),
  SN(File, ALL, "109on104.mayu"	     , ToDest),
  SN(File, ALL, "default.mayu"	     , ToDest),
  SN(File, ALL, "dot.mayu"	     , ToDest),
  SN(File, ALL, "emacsedit.mayu"     , ToDest),
				    
  // documents				    
  SN(Dir , ALL, "doc"	 	     , ToDest), // mkdir
  DN(File, ALL, "banner-ja.png"	     , ToDest, "doc\\banner-ja.png"	 ),
  DN(File, ALL, "edit-setting-ja.png", ToDest, "doc\\edit-setting-ja.png"),
  DN(File, ALL, "investigate-ja.png" , ToDest, "doc\\investigate-ja.png" ),
  DN(File, ALL, "log-ja.png"	     , ToDest, "doc\\log-ja.png"	 ),
  DN(File, ALL, "menu-ja.png"	     , ToDest, "doc\\menu-ja.png"	 ),
  DN(File, ALL, "pause-ja.png"	     , ToDest, "doc\\pause-ja.png"	 ),
  DN(File, ALL, "setting-ja.png"     , ToDest, "doc\\setting-ja.png"	 ),
  DN(File, ALL, "target.png"	     , ToDest, "doc\\target.png"	 ),
  DN(File, ALL, "version-ja.png"     , ToDest, "doc\\version-ja.png"	 ),
  DN(File, ALL, "CONTENTS-ja.html"   , ToDest, "doc\\CONTENTS-ja.html"	 ),
  DN(File, ALL, "CUSTOMIZE-ja.html"  , ToDest, "doc\\CUSTOMIZE-ja.html"	 ),
  DN(File, ALL, "MANUAL-ja.html"     , ToDest, "doc\\MANUAL-ja.html"	 ),
  DN(File, ALL, "README-ja.html"     , ToDest, "doc\\README-ja.html"	 ),
  DN(File, ALL, "README.css"	     , ToDest, "doc\\README.css"	 ),
  DN(File, ALL, "syntax.txt"	     , ToDest, "doc\\syntax.txt"	 ),
  
  SN(File, ALL, "mayu-mode.el"	     , ToDest),
				    
  SN(Dir , ALL, "contrib"	     , ToDest), // mkdir
  DN(File, ALL, "mayu-settings.txt"  , ToDest, "contrib\\mayu-settings.txt"),
  DN(File, ALL, "dvorak.mayu"	     , ToDest, "contrib\\dvorak.mayu"      ),
  DN(File, ALL, "dvorak109.mayu"     , ToDest, "contrib\\dvorak109.mayu"   ),
  DN(File, ALL, "keitai.mayu"	     , ToDest, "contrib\\keitai.mayu"      ),
  DN(File, ALL, "ax.mayu"	     , ToDest, "contrib\\ax.mayu"          ),
  DN(File, ALL, "98x1.mayu"	     , ToDest, "contrib\\98x1.mayu"        ),
  DN(File, ALL, "109onAX.mayu"	     , ToDest, "contrib\\109onAX.mayu"     ),
};


enum KeyboardKind
{
  KEYBOARD_KIND_109,
  KEYBOARD_KIND_104,
} g_keyboardKind;


static const StringResource g_strres[] =
{
#include "strres.h"
};


bool g_wasExecutedBySFX = false;	// Was setup executed by cab32 SFX ?
Resource *g_resource;			// resource information
tstringi g_destDir;			// destination directory


///////////////////////////////////////////////////////////////////////////////
// functions


// show message
int message(int i_id, int i_flag, HWND i_hwnd = NULL)
{
  return MessageBox(i_hwnd, g_resource->loadString(i_id),
		    g_resource->loadString(IDS_mayuSetup), i_flag);
}


// driver service error
void driverServiceError(DWORD i_err)
{
  switch (i_err)
  {
    case ERROR_ACCESS_DENIED:
      message(IDS_notAdministrator, MB_OK | MB_ICONSTOP);
      break;
    case ERROR_SERVICE_MARKED_FOR_DELETE:
      message(IDS_alreadyUninstalled, MB_OK | MB_ICONSTOP);
      break;
    default:
      message(IDS_error, MB_OK | MB_ICONSTOP);
      break;
  }
}


#if defined(_WINNT)

#  define MAYUD_DRIVER_KEY _T("System\\CurrentControlSet\\Services\\mayud")
#  define MAYUD_FILTER_KEY _T("System\\CurrentControlSet\\Control\\Class\\{4D36E96B-E325-11CE-BFC1-08002BE10318}")

bool createUsbDriverServiceExperimentally()
{
  {
    Registry reg(HKEY_LOCAL_MACHINE, MAYUD_DRIVER_KEY);
    reg.write(_T("Type"), DWORD(1));
    reg.write(_T("ErrorControl"), DWORD(1));
    reg.write(_T("Start"), DWORD(3));
  }
  {
    Registry reg(HKEY_LOCAL_MACHINE, MAYUD_FILTER_KEY);
    std::list<tstring> filters;
    if (!reg.read(_T("UpperFilters"), &filters))
      return false;
    filters.push_back(_T("mayud"));
    if (!reg.write(_T("UpperFilters"), filters))
      return false;
  }
  return true;
}

void removeUsbDriverServiceExperimentally()
{
  Registry::remove(HKEY_LOCAL_MACHINE, MAYUD_DRIVER_KEY _T("\\Security"));
  Registry::remove(HKEY_LOCAL_MACHINE, MAYUD_DRIVER_KEY _T("\\Enum"));
  Registry::remove(HKEY_LOCAL_MACHINE, MAYUD_DRIVER_KEY);
  
  Registry reg(HKEY_LOCAL_MACHINE, MAYUD_FILTER_KEY);
  std::list<tstring> filters;
  if (!reg.read(_T("UpperFilters"), &filters))
    return;
  for (std::list<tstring>::iterator
	 i = filters.begin(); i != filters.end(); ++ i)
    if (*i == _T("mayud"))
    {
      filters.erase(i);
      break;
    }
  reg.write(_T("UpperFilters"), filters);
}
#endif


///////////////////////////////////////////////////////////////////////////////
// dialogue


// dialog box
class DlgMain
{
  HWND m_hwnd;
  bool m_doRegisterToStartMenu;	// if register to the start menu
  bool m_doRegisterToStartUp;	// if register to the start up

private:
  // install
  int install()
  {
    Registry reg(DIR_REGISTRY_ROOT);
    CHECK_TRUE( reg.write(_T("dir"), g_destDir) );
    tstringi srcDir = getModuleDirectory();

    if (!installFiles(g_setupFiles, NUMBER_OF(g_setupFiles), g_flags, srcDir,
		      g_destDir))
    {
      removeFiles(g_setupFiles, NUMBER_OF(g_setupFiles), g_flags, g_destDir);
      if (g_wasExecutedBySFX)
	removeSrcFiles(g_setupFiles, NUMBER_OF(g_setupFiles), g_flags, srcDir);
      return 1;
    }
    if (g_wasExecutedBySFX)
      removeSrcFiles(g_setupFiles, NUMBER_OF(g_setupFiles), g_flags, srcDir);

#if defined(_WINNT)
    // driver
    if (g_flags == Flag_UsbTest)
    {
      if (!createUsbDriverServiceExperimentally())
      {
	removeFiles(g_setupFiles, NUMBER_OF(g_setupFiles), g_flags, g_destDir);
	return 1;
      }
      CHECK_TRUE( reg.write(_T("isUsbDriver"), DWORD(1)) );
    }
    else
    {
      DWORD err =
	createDriverService(_T("mayud"),
			    g_resource->loadString(IDS_mayud),
			    getDriverDirectory() + _T("\\mayud.sys"),
			    _T("+Keyboard Class\0"));
      if (err != ERROR_SUCCESS)
      {
	driverServiceError(err);
	removeFiles(g_setupFiles, NUMBER_OF(g_setupFiles), g_flags, g_destDir);
	return 1;
      }
    }
#endif // _WINNT
    
    // create shortcut
    if (m_doRegisterToStartMenu)
    {
      tstringi shortcut = getStartMenuName(loadString(IDS_shortcutName));
      if (!shortcut.empty())
	createLink((g_destDir + _T("\\mayu.exe")).c_str(), shortcut.c_str(),
		   g_resource->loadString(IDS_shortcutName),
		   g_destDir.c_str());
    }
    if (m_doRegisterToStartUp)
    {
      tstringi shortcut = getStartUpName(loadString(IDS_shortcutName));
      if (!shortcut.empty())
	createLink((g_destDir + _T("\\mayu.exe")).c_str(), shortcut.c_str(),
		   g_resource->loadString(IDS_shortcutName),
		   g_destDir.c_str());
    }

    // set registry
    reg.write(_T("layout"),
	      (g_keyboardKind == KEYBOARD_KIND_109) ? _T("109") : _T("104"));

    // file extension
    createFileExtension(_T(".mayu"), _T("text/plain"),
			_T("mayu file"), g_resource->loadString(IDS_mayuFile),
			g_destDir + _T("\\mayu.exe,1"),
			g_resource->loadString(IDS_mayuShellOpen));
    
    // uninstall
    createUninstallInformation(_T("mayu"), g_resource->loadString(IDS_mayu),
			       g_destDir + _T("\\setup.exe -u"));

    if (g_flags == Flag_UsbTest)
    {
      if (message(IDS_copyFinishUsb, MB_YESNO | MB_ICONQUESTION, m_hwnd)
	  == IDYES)
      {
	// reboot ...
	HANDLE hToken; 
	// Get a token for this process. 
	if (!OpenProcessToken(GetCurrentProcess(), 
			      TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
	{
	  message(IDS_failedToReboot, MB_OK | MB_ICONSTOP);
	  return 0;
	}
	// Get the LUID for the shutdown privilege.
	TOKEN_PRIVILEGES tkp;
	LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);
	tkp.PrivilegeCount = 1;  // one privilege to set
	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	// Get the shutdown privilege for this process. 
	AdjustTokenPrivileges(hToken, FALSE, &tkp, 0,
			      (PTOKEN_PRIVILEGES)NULL, 0);
	// Cannot test the return value of AdjustTokenPrivileges. 
	if (GetLastError() != ERROR_SUCCESS)
	{
	  message(IDS_failedToReboot, MB_OK | MB_ICONSTOP);
	  return 0;
	}
	// Shut down the system and force all applications to close. 
	if (!ExitWindowsEx(EWX_REBOOT | EWX_FORCE, 0))
	{
	  message(IDS_failedToReboot, MB_OK | MB_ICONSTOP);
	  return 0;
	}
      }
    }
    else
    {
      if (message(IDS_copyFinish, MB_YESNO | MB_ICONQUESTION, m_hwnd)
	  == IDYES)
	ExitWindows(0, 0);			// logoff
    }
    return 0;
  }
  
private:
  // WM_INITDIALOG
  BOOL wmInitDialog(HWND /* focus */, LPARAM /* lParam */)
  {
    setSmallIcon(m_hwnd, IDI_ICON_mayu);
    setBigIcon(m_hwnd, IDI_ICON_mayu);
    Edit_SetText(GetDlgItem(m_hwnd, IDC_EDIT_path), g_destDir.c_str());
    HWND hwndCombo = GetDlgItem(m_hwnd, IDC_COMBO_keyboard);
    ComboBox_AddString(hwndCombo, g_resource->loadString(IDS_keyboard109));
    ComboBox_AddString(hwndCombo, g_resource->loadString(IDS_keyboard104));
    if (checkOs(SetupFile::W2k))
    {
      ComboBox_AddString(hwndCombo,
			 g_resource->loadString(IDS_keyboard109usb));
      ComboBox_AddString(hwndCombo,
			 g_resource->loadString(IDS_keyboard104usb));
    }
    ComboBox_SetCurSel(hwndCombo,
		       (g_keyboardKind == KEYBOARD_KIND_109) ? 0 : 1);
    tstring note = g_resource->loadString(IDS_note01);
    note += g_resource->loadString(IDS_note02);
    note += g_resource->loadString(IDS_note03);
    note += g_resource->loadString(IDS_note04);
    Edit_SetText(GetDlgItem(m_hwnd, IDC_EDIT_note), note.c_str());
    return TRUE;
  }
  
  // WM_CLOSE
  BOOL wmClose()
  {
    EndDialog(m_hwnd, 0);
    return TRUE;
  }
  
  // WM_COMMAND
  BOOL wmCommand(int /* notify_code */, int i_id, HWND /* hwnd_control */)
  {
    switch (i_id)
    {
      case IDC_BUTTON_browse:
      {
	_TCHAR folder[GANA_MAX_PATH];
	
	BROWSEINFO bi;
	ZeroMemory(&bi, sizeof(bi));
	bi.hwndOwner      = m_hwnd;
	bi.pidlRoot       = NULL;
	bi.pszDisplayName = folder;
	bi.lpszTitle      = g_resource->loadString(IDS_selectDir);
	ITEMIDLIST *browse = SHBrowseForFolder(&bi);
	if (browse != NULL)
	{
	  if (SHGetPathFromIDList(browse, folder))
	  {
	    if (createDirectories(folder))
	      Edit_SetText(GetDlgItem(m_hwnd, IDC_EDIT_path), folder);
	  }
	  IMalloc *imalloc = NULL;
	  if (SHGetMalloc(&imalloc) == NOERROR)
	    imalloc->Free((void *)browse);
	}
	return TRUE;
      }
      
      case IDOK:
      {
	_TCHAR buf[GANA_MAX_PATH];
	Edit_GetText(GetDlgItem(m_hwnd, IDC_EDIT_path), buf, NUMBER_OF(buf));
	if (buf[0])
	{
	  g_destDir = normalizePath(buf);
	  m_doRegisterToStartMenu =
	    (IsDlgButtonChecked(m_hwnd, IDC_CHECK_registerStartMenu) ==
	     BST_CHECKED);
	  m_doRegisterToStartUp =
	    (IsDlgButtonChecked(m_hwnd, IDC_CHECK_registerStartUp) ==
	     BST_CHECKED);
	  switch (ComboBox_GetCurSel(GetDlgItem(m_hwnd, IDC_COMBO_keyboard)))
	  {
	    case 0:
	      g_keyboardKind = KEYBOARD_KIND_109;
	      g_flags = SetupFile::Normal;
	      break;
	    case 1:
	      g_keyboardKind = KEYBOARD_KIND_104;
	      g_flags = SetupFile::Normal;
	      break;
	    case 2:
	      g_keyboardKind = KEYBOARD_KIND_109;
	      g_flags = Flag_UsbTest;
	      break;
	    case 3:
	      g_keyboardKind = KEYBOARD_KIND_104;
	      g_flags = Flag_UsbTest;
	      break;
	  };

	  if (g_flags == Flag_UsbTest)
	    if (message(IDS_usbWarning, MB_OKCANCEL | MB_ICONWARNING, m_hwnd)
		== IDCANCEL)
	      return TRUE;
	  
	  if (createDirectories(g_destDir.c_str()))
	    EndDialog(m_hwnd, install());
	  else
	    message(IDS_invalidDirectory, MB_OK | MB_ICONSTOP, m_hwnd);
	}
	else
	  message(IDS_mayuEmpty, MB_OK, m_hwnd);
	return TRUE;
      }
      
      case IDCANCEL:
      {
	CHECK_TRUE( EndDialog(m_hwnd, 0) );
	return TRUE;
      }
    }
    return FALSE;
  }

public:
  DlgMain(HWND i_hwnd)
    : m_hwnd(i_hwnd),
      m_doRegisterToStartMenu(false),
      m_doRegisterToStartUp(false)
  {
  }

  static BOOL CALLBACK dlgProc(HWND i_hwnd, UINT i_message,
			       WPARAM i_wParam, LPARAM i_lParam)
  {
    DlgMain *wc;
    getUserData(i_hwnd, &wc);
    if (!wc)
      switch (i_message)
      {
	case WM_INITDIALOG:
	  wc = setUserData(i_hwnd, new DlgMain(i_hwnd));
	  return wc->wmInitDialog(reinterpret_cast<HWND>(i_wParam), i_lParam);
      }
    else
      switch (i_message)
      {
	case WM_COMMAND:
	  return wc->wmCommand(HIWORD(i_wParam), LOWORD(i_wParam),
			       reinterpret_cast<HWND>(i_lParam));
	case WM_CLOSE:
	  return wc->wmClose();
	case WM_NCDESTROY:
	  delete wc;
	  return TRUE;
      }
    return FALSE;
  }
};


// uninstall
// (in this function, we cannot use any resource, so we use strres[])
int uninstall()
{
  if (IDYES != message(IDS_removeOk, MB_YESNO | MB_ICONQUESTION))
    return 1;

#if defined(_WINNT)
  int isUsbDriver;
  Registry::read(DIR_REGISTRY_ROOT, _T("isUsbDriver"), &isUsbDriver, 0);
  if (isUsbDriver)
    g_flags = Flag_UsbTest;
  if (g_flags == Flag_UsbTest)
    removeUsbDriverServiceExperimentally();
  else
  {
    DWORD err = removeDriverService(_T("mayud"));
    if (err != ERROR_SUCCESS)
    {
      driverServiceError(err);
      return 1;
    }
  }
#endif // _WINNT

  DeleteFile(getStartMenuName(
    g_resource->loadString(IDS_shortcutName)).c_str());
  DeleteFile(getStartUpName(
    g_resource->loadString(IDS_shortcutName)).c_str());

  removeFiles(g_setupFiles, NUMBER_OF(g_setupFiles), g_flags, g_destDir);
  removeFileExtension(_T(".mayu"), _T("mayu file"));
  removeUninstallInformation(_T("mayu"));
  
  Registry::remove(DIR_REGISTRY_ROOT);
  Registry::remove(HKEY_CURRENT_USER, _T("Software\\GANAware\\mayu"));
  
  message(IDS_removeFinish, MB_OK | MB_ICONINFORMATION);
  return 0;
}


int WINAPI _tWinMain(HINSTANCE i_hInstance, HINSTANCE /* hPrevInstance */,
		     LPTSTR /* lpszCmdLine */, int /* nCmdShow */)
{
  CoInitialize(NULL);
  
  g_hInst = i_hInstance;
  Resource resource(g_strres);
  g_resource = &resource;

  // check OS
  if (
#if defined(_WINNT)
      !checkOs(SetupFile::NT)
#elif defined(_WIN95)
      !checkOs(SetupFile::W9x)
#else
#  error
#endif
    )
  {
    message(IDS_invalidOS, MB_OK | MB_ICONSTOP);
    return 1;
  }

  // keyboard kind
  g_keyboardKind =
    (resource.getLocale() == LOCALE_Japanese_Japan_932) ?
    KEYBOARD_KIND_109 : KEYBOARD_KIND_104;

  // read registry
  tstringi programFiles;			// "Program Files" directory
  Registry::read(HKEY_LOCAL_MACHINE,
		 _T("Software\\Microsoft\\Windows\\CurrentVersion"),
		 _T("ProgramFilesDir"), &programFiles);
  Registry::read(DIR_REGISTRY_ROOT, _T("dir"), &g_destDir,
		 programFiles + _T("\\mayu"));

  int retval = 1;
  
  if (__argc == 2 && _tcsicmp(__targv[1], _T("-u")) == 0)
    retval = uninstallStep1(_T("-u"));
  else
  {
    // is mayu running ?
    HANDLE mutex = CreateMutex(NULL, TRUE, MUTEX_MAYU_EXCLUSIVE_RUNNING);
    if (GetLastError() == ERROR_ALREADY_EXISTS) // mayu is running
      message(IDS_mayuRunning, MB_OK | MB_ICONSTOP);
    else if (__argc == 3 && _tcsicmp(__targv[1], _T("-u")) == 0)
    {
      uninstallStep2(__targv[2]);
      retval = uninstall();
    }
    else if (__argc == 2 && _tcsicmp(__targv[1], _T("-s")) == 0)
    {
      g_wasExecutedBySFX = true;
      retval = DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_main), NULL,
			 DlgMain::dlgProc);
    }
    else if (__argc == 1)
      retval = DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIALOG_main), NULL,
			 DlgMain::dlgProc);
    CloseHandle(mutex);
  }
  
  return retval;
}
