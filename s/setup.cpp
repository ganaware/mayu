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
	"Software\\GANAware\\mayu"


///////////////////////////////////////////////////////////////////////////////
// Globals


typedef SetupFile SF;

const SetupFile setupFiles[] =
{
  // executables
  SetupFile(SF::Dll , SF::NT , "mayu.dll"       , SF::ToDest),
  SetupFile(SF::File, SF::NT , "mayu.exe"       , SF::ToDest),
  SetupFile(SF::File, SF::NT , "setup.exe"      , SF::ToDest),

  // drivers
  SetupFile(SF::File, SF::NT , "mayud.sys"      , SF::ToDest),
  SetupFile(SF::File, SF::NT , "mayudnt4.sys"   , SF::ToDest),
  SetupFile(SF::File, SF::W2k, "mayud.sys"      , SF::ToDriver, "mayud.sys"),
  SetupFile(SF::File, SF::NT4, "mayudnt4.sys"   , SF::ToDriver, "mayud.sys"),

  // setting files
  SetupFile(SF::File, SF::NT , "104.mayu"       , SF::ToDest),
  SetupFile(SF::File, SF::NT , "104on109.mayu"  , SF::ToDest),
  SetupFile(SF::File, SF::NT , "109.mayu"       , SF::ToDest),
  SetupFile(SF::File, SF::NT , "109on104.mayu"  , SF::ToDest),
  SetupFile(SF::File, SF::NT , "default.mayu"   , SF::ToDest),
  SetupFile(SF::File, SF::NT , "dot.mayu"       , SF::ToDest),
  SetupFile(SF::File, SF::NT , "emacsedit.mayu" , SF::ToDest),

  // documents
  SetupFile(SF::File, SF::NT , "CONTENTS.html"  , SF::ToDest),
  SetupFile(SF::File, SF::NT , "CUSTOMIZE.html" , SF::ToDest),
  SetupFile(SF::File, SF::NT , "MANUAL.html"    , SF::ToDest),
  SetupFile(SF::File, SF::NT , "README.css"     , SF::ToDest),
  SetupFile(SF::File, SF::NT , "README.html"    , SF::ToDest),
  SetupFile(SF::File, SF::NT , "mayu-banner.png", SF::ToDest),
  SetupFile(SF::File, SF::NT , "syntax.txt"     , SF::ToDest),
  SetupFile(SF::File, SF::NT , "mayu-mode.el"   , SF::ToDest),

  SetupFile(SF::File, SF::NT , "source.cab"     , SF::ToDest),

  SetupFile(SF::Dir , SF::NT , "contrib"        , SF::ToDest),	// mkdir
  SetupFile(SF::File, SF::NT , "contrib\\mayu-settings.txt", SF::ToDest),
  SetupFile(SF::File, SF::NT , "contrib\\dvorak.mayu"      , SF::ToDest),
  SetupFile(SF::File, SF::NT , "contrib\\keitai.mayu"      , SF::ToDest),
  SetupFile(SF::File, SF::NT , "contrib\\ax.mayu"          , SF::ToDest),
};


enum KeyboardKind
{
  KeyboardKind109,
  KeyboardKind104,
} keyboardKind;


static const StringResource strres[] =
{
#include "strres.h"
};


bool wasExecutedBySFX = false;		// Was setup executed by cab32 SFX ?
Resource *resource;			// resource information
istring destDir;			// destination directory


///////////////////////////////////////////////////////////////////////////////
// functions


// show message
int message(int id, int flag, HWND hwnd = NULL)
{
  return MessageBox(hwnd, resource->loadString(id),
		    resource->loadString(IDS_mayuSetup), flag);
}


// driver service error
void driverServiceError(DWORD err)
{
  switch (err)
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


///////////////////////////////////////////////////////////////////////////////
// dialogue


// dialog box
class DlgMain
{
  HWND hwnd;
  bool doRegisterToStartMenu;	// if register to the start menu
  bool doRegisterToStartUp;	// if register to the start up

private:
  // install
  int install()
  {
    Registry reg(DIR_REGISTRY_ROOT);
    _true( reg.write("dir", destDir) );
    istring srcDir = getModuleDirectory();

    if (!installFiles(setupFiles, lengthof(setupFiles), srcDir, destDir))
    {
      removeFiles(setupFiles, lengthof(setupFiles), destDir);
      if (wasExecutedBySFX)
	removeSrcFiles(setupFiles, lengthof(setupFiles), srcDir);
      return 1;
    }
    if (wasExecutedBySFX)
      removeSrcFiles(setupFiles, lengthof(setupFiles), srcDir);

    // driver
    DWORD err =
      createDriverService("mayud",
			  resource->loadString(IDS_mayud),
			  getDriverDirectory() + "\\mayud.sys",
			  "+Keyboard Class\0");
    if (err != ERROR_SUCCESS)
    {
      driverServiceError(err);
      removeFiles(setupFiles, lengthof(setupFiles), destDir);
      return 1;
    }
    
    // create shortcut
    if (doRegisterToStartMenu)
    {
      istring shortcut = getStartMenuName(loadString(IDS_shortcutName));
      if (!shortcut.empty())
	createLink((destDir + "\\mayu.exe").c_str(), shortcut.c_str(),
		   resource->loadString(IDS_shortcutName));
    }
    if (doRegisterToStartUp)
    {
      istring shortcut = getStartUpName(loadString(IDS_shortcutName));
      if (!shortcut.empty())
	createLink((destDir + "\\mayu.exe").c_str(), shortcut.c_str(),
		   resource->loadString(IDS_shortcutName));
    }

    // set registry
    reg.write("layout", (keyboardKind == KeyboardKind109) ? "109" : "104");

    // file extension
    createFileExtension(".mayu", "text/plain",
			"mayu file", resource->loadString(IDS_mayuFile),
			destDir + "\\mayu.exe,1",
			resource->loadString(IDS_mayuShellOpen));
    
    // uninstall
    createUninstallInformation("mayu", resource->loadString(IDS_mayu),
			       destDir + "\\setup.exe -u");

    if (message(IDS_copyFinish, MB_YESNO | MB_ICONQUESTION, hwnd)
	== IDYES)
      ExitWindows(0, 0);			// logoff
    return 0;
  }
  
private:
  // WM_INITDIALOG
  BOOL wmInitDialog(HWND /* focus */, LPARAM /* lParam */)
  {
    setSmallIcon(hwnd, IDI_ICON_mayu);
    setBigIcon(hwnd, IDI_ICON_mayu);
    Edit_SetText(GetDlgItem(hwnd, IDC_EDIT_path), destDir.c_str());
    HWND hwndCombo = GetDlgItem(hwnd, IDC_COMBO_keyboard);
    ComboBox_AddString(hwndCombo, resource->loadString(IDS_keyboard109));
    ComboBox_AddString(hwndCombo, resource->loadString(IDS_keyboard104));
    ComboBox_SetCurSel(hwndCombo,
		       (keyboardKind == KeyboardKind109) ? 0 : 1);
    string note = resource->loadString(IDS_note01);
    note += resource->loadString(IDS_note02);
    note += resource->loadString(IDS_note03);
    note += resource->loadString(IDS_note04);
    Edit_SetText(GetDlgItem(hwnd, IDC_EDIT_note), note.c_str());
    return TRUE;
  }
  
  // WM_CLOSE
  BOOL wmClose()
  {
    EndDialog(hwnd, 0);
    return TRUE;
  }
  
  // WM_COMMAND
  BOOL wmCommand(int /* notify_code */, int id, HWND /* hwnd_control */)
  {
    switch (id)
    {
      case IDC_BUTTON_browse:
      {
	char folder[GANA_MAX_PATH];
	
	BROWSEINFO bi;
	ZeroMemory(&bi, sizeof(bi));
	bi.hwndOwner      = hwnd;
	bi.pidlRoot       = NULL;
	bi.pszDisplayName = folder;
	bi.lpszTitle      = resource->loadString(IDS_selectDir);
	ITEMIDLIST *browse = SHBrowseForFolder(&bi);
	if (browse != NULL)
	{
	  if (SHGetPathFromIDList(browse, folder))
	  {
	    if (createDirectories(folder))
	      Edit_SetText(GetDlgItem(hwnd, IDC_EDIT_path), folder);
	  }
	  IMalloc *imalloc = NULL;
	  if (SHGetMalloc(&imalloc) == NOERROR)
	    imalloc->Free((void *)browse);
	}
	return TRUE;
      }
      
      case IDOK:
      {
	char buf[GANA_MAX_PATH];
	Edit_GetText(GetDlgItem(hwnd, IDC_EDIT_path), buf, lengthof(buf));
	if (buf[0])
	{
	  destDir = normalizePath(buf);
	  doRegisterToStartMenu =
	    (IsDlgButtonChecked(hwnd, IDC_CHECK_registerStartMenu) ==
	     BST_CHECKED);
	  doRegisterToStartUp =
	    (IsDlgButtonChecked(hwnd, IDC_CHECK_registerStartUp) ==
	     BST_CHECKED);
	  switch (ComboBox_GetCurSel(GetDlgItem(hwnd, IDC_COMBO_keyboard)))
	  {
	    case 0: keyboardKind = KeyboardKind109; break;
	    case 1: keyboardKind = KeyboardKind104; break;
	  };
	  
	  if (createDirectories(destDir.c_str()))
	    EndDialog(hwnd, install());
	  else
	    message(IDS_invalidDirectory, MB_OK | MB_ICONSTOP, hwnd);
	}
	else
	  message(IDS_mayuEmpty, MB_OK, hwnd);
	return TRUE;
      }
      
      case IDCANCEL:
      {
	_true( EndDialog(hwnd, 0) );
	return TRUE;
      }
    }
    return FALSE;
  }

public:
  DlgMain(HWND hwnd_)
    : hwnd(hwnd_),
      doRegisterToStartMenu(false),
      doRegisterToStartUp(false)
  {
  }

  static BOOL CALLBACK dlgProc(HWND hwnd, UINT message,
			       WPARAM wParam, LPARAM lParam)
  {
    DlgMain *wc;
    getUserData(hwnd, &wc);
    if (!wc)
      switch (message)
      {
	case WM_INITDIALOG:
	  wc = setUserData(hwnd, new DlgMain(hwnd));
	  return wc->wmInitDialog((HWND)wParam, lParam);
      }
    else
      switch (message)
      {
	case WM_COMMAND:
	  return wc->wmCommand(HIWORD(wParam), LOWORD(wParam), (HWND)lParam);
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

  DWORD err = removeDriverService("mayud");
  if (err != ERROR_SUCCESS)
  {
    driverServiceError(err);
    return 1;
  }

  DeleteFile(getStartMenuName(resource->loadString(IDS_shortcutName)).c_str());
  DeleteFile(getStartUpName(resource->loadString(IDS_shortcutName)).c_str());

  removeFiles(setupFiles, lengthof(setupFiles), destDir);
  removeFileExtension(".mayu", "mayu file");
  removeUninstallInformation("mayu");
  
  Registry::remove(DIR_REGISTRY_ROOT);
  Registry::remove(HKEY_CURRENT_USER, "Software\\GANAware\\mayu");
  
  message(IDS_removeFinish, MB_OK | MB_ICONINFORMATION);
  return 0;
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /* hPrevInstance */,
		   LPSTR /* lpszCmdLine */, int /* nCmdShow */)
{
  CoInitialize(NULL);
  
  hInst = hInstance;
  Resource resource(strres);
  ::resource = &resource;

  // check OS
  if (!checkOs(SetupFile::NT))
  {
    message(IDS_invalidOS, MB_OK | MB_ICONSTOP);
    return 1;
  }

  // keyboard kind
  keyboardKind =
    (resource.getLocale() == Locale_Japanese_Japan_932) ?
    KeyboardKind109 : KeyboardKind104;

  // read registry
  istring programFiles;			// "Program Files" directory
  Registry::read(HKEY_LOCAL_MACHINE,
		 "Software\\Microsoft\\Windows\\CurrentVersion",
		 "ProgramFilesDir", &programFiles);
  Registry::read(DIR_REGISTRY_ROOT, "dir", &destDir, programFiles + "\\mayu");

  int retval = 1;
  
  if (__argc == 2 && mbsiequal_(__argv[1], "-u"))
    retval = uninstallStep1("-u");
  else
  {
    // is mayu running ?
    HANDLE mutex = CreateMutex(NULL, TRUE, mutexMayuExclusiveRunning);
    if (GetLastError() == ERROR_ALREADY_EXISTS) // mayu is running
      message(IDS_mayuRunning, MB_OK | MB_ICONSTOP);
    else if (__argc == 3 && mbsiequal_(__argv[1], "-u"))
    {
      uninstallStep2(__argv[2]);
      retval = uninstall();
    }
    else if (__argc == 2 && mbsiequal_(__argv[1], "-s"))
    {
      wasExecutedBySFX = true;
      retval = DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG_main), NULL,
			 DlgMain::dlgProc);
    }
    else if (__argc == 1)
      retval = DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG_main), NULL,
			 DlgMain::dlgProc);
    CloseHandle(mutex);
  }
  
  return retval;
}
