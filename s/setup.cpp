///////////////////////////////////////////////////////////////////////////////
// setup.cpp


#include "../misc.h"

#include "../registry.h"
#include "../stringtool.h"
#include "../windowstool.h"
#include "../mayu.h"
#include "setuprc.h"

#include <windowsx.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>


using namespace std;
using StringTool::istring;


///////////////////////////////////////////////////////////////////////////////
// Global variables


#define UNINSTALL_REGISTRY_ROOT HKEY_LOCAL_MACHINE, \
"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\mayu"
#define DIR_REGISTRY_ROOT HKEY_LOCAL_MACHINE, "Software\\GANAware\\mayu"
#define PROGRAM_FILES_REGISTRY HKEY_LOCAL_MACHINE, \
"Software\\Microsoft\\Windows\\CurrentVersion", "ProgramFilesDir"

istring dir;				// directory to install
bool doRegisterToStartMenu = false;	// if register to the start menu
bool doRegisterToStartUp = false;	// if register to the start up
enum {
  keyboard109,
  keyboard104,
} keyboardKind = keyboard109;

// files to copy
const char *setupFiles[] =
{
  "mayu.exe",
  "mayu.dll",
  "mayud.sys",
  "mayudnt4.sys",
  "setup.exe",
  "default.mayu",
  "emacsedit.mayu",
  "dot.mayu",
  "109.mayu",
  "104.mayu",
  "109on104.mayu",
  "104on109.mayu",
  "syntax.txt",
  "README.html",
  "README.css",
  "CONTENTS.html",
  "MANUAL.html",
  "CUSTOMIZE.html",
  "source.cab",
  "contrib",
};

// files to copy driver's directory
const char *setupFilesDriver[] =
{
  "mayud.sys",		// driver for Windows2000
  "mayudnt4.sys",	// driver for Windows NT4.0
};

// remove only files
const char *removeOnlyFiles[] =
{
  "_mayu.dll",
  "contrib\\mayu-settings.txt",
  "contrib\\dvorak.mayu",
};

// remove only dirs
const char *removeOnlyDirs[] =
{
  "contrib",
};

// string resources
const struct
{
  UINT id;
  char *str;
} strres[] = {
#include "strres.h"
  0, NULL,
};


///////////////////////////////////////////////////////////////////////////////
// Functions


istring getDriverDirectory();


const char *getString(UINT id)
{
  for (int i = 0; strres[i].str; i++)
    if (strres[i].id == id)
      return strres[i].str;
  return "";
}


// show message
static int message(int id, int flag, HWND hwnd = NULL)
{
  return MessageBox(hwnd, getString(id), getString(IDS_mayuSetup), flag);
}


// fix path (discard src)
static char *pathFix(char *buf)
{
  char *s;
  for (s = buf; *s; s ++)
    if (*s == '/')
      *s = '\\';
    else if (StringTool::ismbblead_(*s) && s[1])
      s ++;
  PathRemoveBackslash(buf);

  char tmp[GANA_MAX_PATH];
  char *d = tmp;
  s = buf;
  if (*s == '\\')
  {
    *(d ++) = '\\';
    s ++;
  }
  while (*s)
    if (*s == '\\' && *(s + 1) == '\\')
      s ++;
    else
      *(d ++) = *(s ++);
  *d = '\0';
  
  PathCanonicalize(buf, tmp);
  return buf;
}


// CreateLink - uses the shell's IShellLink and IPersistFile interfaces 
//   to create and store a shortcut to the specified object. 
// Returns the result of calling the member functions of the interfaces. 
// lpszPathObj - address of a buffer containing the path of the object. 
// lpszPathLink - address of a buffer containing the path where the 
//   shell link is to be stored. 
// lpszDesc - address of a buffer containing the description of the 
//   shell link. 
HRESULT CreateLink(LPCSTR lpszPathObj, LPCSTR lpszPathLink, LPCSTR lpszDesc)
{ 
  HRESULT hres;
  IShellLink* psl;

  // Get a pointer to the IShellLink interface. 
  hres = CoCreateInstance(CLSID_ShellLink, NULL, 
			  CLSCTX_INPROC_SERVER, IID_IShellLink, (void **)&psl);
  if (SUCCEEDED(hres))
  { 
    // Set the path to the shortcut target and add the description. 
    psl->SetPath(lpszPathObj);
    psl->SetDescription(lpszDesc);
 
    // Query IShellLink for the IPersistFile interface for saving the 
    // shortcut in persistent storage. 
    IPersistFile* ppf; 
    hres = psl->QueryInterface(IID_IPersistFile, (void **)&ppf);
 
    if (SUCCEEDED(hres))
    {
      wchar_t wsz[MAX_PATH]; 
      
      // Ensure that the string is ANSI. 
      MultiByteToWideChar(CP_ACP, 0, lpszPathLink, -1, wsz, MAX_PATH); 
      
      // Save the link by calling IPersistFile::Save. 
      hres = ppf->Save(wsz, TRUE); 
      ppf->Release();
    } 
    psl->Release();
  } 
  return hres; 
} 



// create mayu file extension information
static void createMayuFileExtenson()
{
  string dummy;
  if (!Registry::read(HKEY_CLASSES_ROOT, ".mayu", "", &dummy))
    _true( Registry::write(HKEY_CLASSES_ROOT, ".mayu", "", "mayu file") );
  if (!Registry::read(HKEY_CLASSES_ROOT, ".mayu", "Content Type", &dummy))
    _true( Registry::write(HKEY_CLASSES_ROOT, ".mayu", "Content Type",
			   "text/plain") );
  if (!Registry::read(HKEY_CLASSES_ROOT, "mayu file", "", &dummy))
    _true( Registry::write(HKEY_CLASSES_ROOT, "mayu file", "",
			   getString(IDS_mayuFile)) );
  if (!Registry::read(HKEY_CLASSES_ROOT, "mayu file\\DefaultIcon", "", &dummy))
    _true( Registry::write(HKEY_CLASSES_ROOT, "mayu file\\DefaultIcon", "",
			   dir + "\\mayu.exe,1") );
  if (!Registry::read(HKEY_CLASSES_ROOT, "mayu file\\shell\\open\\command",
		      "", &dummy))
    _true( Registry::write(HKEY_CLASSES_ROOT,"mayu file\\shell\\open\\command",
			   "", getString(IDS_mayuShellOpen)) );
}


// remove mayu file extension information
static void removeMayuFileExtenson()
{
  Registry::remove(HKEY_CLASSES_ROOT, ".mayu");
  Registry::remove(HKEY_CLASSES_ROOT, "mayu file\\shell\\open\\command");
  Registry::remove(HKEY_CLASSES_ROOT, "mayu file\\shell\\open");
  Registry::remove(HKEY_CLASSES_ROOT, "mayu file\\shell");
  Registry::remove(HKEY_CLASSES_ROOT, "mayu file");
}


// create uninstallation information
static void createUninstallInformation()
{
  _true( Registry::write(UNINSTALL_REGISTRY_ROOT, "DisplayName",
			 getString(IDS_mayu)) );
  _true( Registry::write(UNINSTALL_REGISTRY_ROOT, "UninstallString",
			 dir + "\\setup.exe -u") );
}


// remove uninstallation information
static void removeUninstallInformation()
{
  Registry::remove(UNINSTALL_REGISTRY_ROOT);
}


// create mayud service
static bool createService()
{
  SC_HANDLE hscm =
    OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE | SC_MANAGER_CONNECT);
  if (!hscm)
    return false;

  istring mayud = getDriverDirectory() + "\\mayud.sys";
  SC_HANDLE hs =
    CreateService(hscm, "mayud", getString(IDS_mayud),
		  SERVICE_START | SERVICE_STOP, SERVICE_KERNEL_DRIVER,
		  SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE,
		  mayud.c_str(), NULL, NULL, NULL, NULL, NULL);
  if (hs == NULL)
  {
    switch (GetLastError())
    {
      case ERROR_SERVICE_EXISTS:
	break;
      case ERROR_ACCESS_DENIED:
	message(IDS_notAdministrator, MB_OK | MB_ICONSTOP);
	return false;
      case ERROR_SERVICE_MARKED_FOR_DELETE:
	message(IDS_alreadyUninstalled, MB_OK | MB_ICONSTOP);
	return false;
      default:
	message(IDS_error, MB_OK | MB_ICONSTOP);
	return false;
    }
  }
  CloseServiceHandle(hs);
  CloseServiceHandle(hscm);
  return true;
}


// remove service
static bool removeService()
{
  SC_HANDLE hscm = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
  SC_HANDLE hs =
    OpenService(hscm, "mayud", SERVICE_START | SERVICE_STOP | DELETE);
  if (!hs)
  {
    switch (GetLastError())
    {
      case ERROR_ACCESS_DENIED:
	message(IDS_notAdministrator, MB_OK | MB_ICONSTOP);
	return false;
      default:
	message(IDS_error, MB_OK | MB_ICONSTOP);
	return false;
    }
  }

  if (!DeleteService(hs))
  {
    switch (GetLastError())
    {
      case ERROR_ACCESS_DENIED:
	message(IDS_notAdministrator, MB_OK | MB_ICONSTOP);
	return false;
      case ERROR_SERVICE_MARKED_FOR_DELETE:
	message(IDS_alreadyUninstalled, MB_OK | MB_ICONSTOP);
	return true;
      default:
	message(IDS_error, MB_OK | MB_ICONSTOP);
	return false;
    }
  }
  return true;
}



// create deep directory
static bool createDirectories(const char *folder)
{
  const char *s = strchr(folder, '\\'); // TODO: '/'
  while (s)
  {
    istring f(folder, 0, s - folder);
    if (!PathIsDirectory(f.c_str()))
      if (!CreateDirectory(f.c_str(), NULL))
	return false;
    s = strchr(s + 1, '\\');
  }
  if (!PathIsDirectory(folder))
    if (!CreateDirectory(folder, NULL))
	return false;
  return true;
}


// get driver directory
istring getDriverDirectory()
{
  char buf[GANA_MAX_PATH];
  _true( GetSystemDirectory(buf, sizeof(buf)) );
  return istring(buf) + "\\drivers";
}


// get current directory
istring getCurrentDirectory()
{
  char buf[GANA_MAX_PATH];
  _true( GetModuleFileName(hInst, buf, sizeof(buf)) );
  _true( PathRemoveFileSpec(buf) );
  return istring(buf);
}


// get start menu name
istring getStartMenuName()
{
  /*
  char programDir[GANA_MAX_PATH];
  if (SUCCEEDED(SHGetSpecialFolderPath(NULL, programDir,
				       CSIDL_COMMON_PROGRAMS, FALSE)))
				       return istring(programDir) + "\\" + getString(IDS_shortcutName) + ".lnk";*/
  istring programDir;
  if (Registry::read(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Common Programs", &programDir))
    return programDir + "\\" + getString(IDS_shortcutName) + ".lnk";
  return "";
}


// get start up name
istring getStartUpName()
{
  istring startupDir;
  if (Registry::read(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Startup", &startupDir))
    return startupDir + "\\" + getString(IDS_shortcutName) + ".lnk";
  return "";
}


// is Windows NT 4.0 ?
bool isNT4()
{
  OSVERSIONINFO ver;
  ver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  GetVersionEx(&ver);
  return
    ver.dwPlatformId == VER_PLATFORM_WIN32_NT &&
    ver.dwMajorVersion == 4;
}


// install files
bool installFiles()
{
  istring fromDir = getCurrentDirectory();
  istring driverTo = getDriverDirectory();
  
  istring to, from;
  SHFILEOPSTRUCT fo;

  // copy
  if (fromDir != dir)
  {
    // move mayu.dll
    from = dir + "\\mayu.dll";
    to = dir + "\\_mayu.dll";
    DeleteFile(to.c_str());
    MoveFile(from.c_str(), to.c_str());

    from = "";
    for (int i = 0; i < lengthof(setupFiles); i++)
    {
      from += fromDir + '\\' + setupFiles[i];
      from += '\0';
    }
    to = dir + '\\';
    
    ZeroMemory(&fo, sizeof(fo));
    fo.wFunc = FO_COPY;
    fo.pFrom = from.c_str(); 
    fo.pTo = to.c_str(); 
    if (SHFileOperation(&fo) || fo.fAnyOperationsAborted)
      return false;
  }

  // copy driver
  from = fromDir + '\\' + setupFilesDriver[isNT4() ? 1 : 0];
  from += '\0';
  to = driverTo + "\\mayud.sys";
  to += '\0';
  
  ZeroMemory(&fo, sizeof(fo));
  fo.wFunc = FO_COPY;
  fo.pFrom = from.c_str(); 
  fo.pTo = to.c_str(); 
  if (SHFileOperation(&fo) || fo.fAnyOperationsAborted)
    return false;

  // create shortcut
  if (doRegisterToStartMenu)
  {
    CoInitialize(NULL);
    istring shortcut = getStartMenuName();
    if (!shortcut.empty())
    {
      istring target = dir + "\\mayu.exe";
      CreateLink(target.c_str(), shortcut.c_str(),
		 getString(IDS_shortcutName));
    }
  }
  
  if (doRegisterToStartUp)
  {
    CoInitialize(NULL);
    istring shortcut = getStartUpName();
    if (!shortcut.empty())
    {
      istring target = dir + "\\mayu.exe";
      CreateLink(target.c_str(), shortcut.c_str(),
		 getString(IDS_shortcutName));
    }
  }

  // set registry
  Registry reg(DIR_REGISTRY_ROOT);
  reg.write("layout", (keyboardKind == keyboard104) ? "104" : "109");
  
  return true;
}


// remove files
void removeFiles()
{
  int i;
  istring driverDir = getDriverDirectory();
  for (i = 0; i < lengthof(setupFilesDriver); i++)
  {
    istring file = driverDir + "\\" + setupFilesDriver[i];
    DeleteFile(file.c_str());
  }
  for (i = 0; i < lengthof(setupFiles); i++)
  {
    istring file = dir + "\\" + setupFiles[i];
    DeleteFile(file.c_str());
  }
  for (i = 0; i < lengthof(removeOnlyFiles); i++)
  {
    istring file = dir + "\\" + removeOnlyFiles[i];
    DeleteFile(file.c_str());
  }
  for (i = 0; i < lengthof(removeOnlyDirs); i ++)
  {
    istring rd = dir + "\\" + removeOnlyDirs[i];
    RemoveDirectory(rd.c_str());
  }

  istring shotcut = getStartMenuName();
  DeleteFile(shotcut.c_str());

  shotcut = getStartUpName();
  DeleteFile(shotcut.c_str());
}


// dialog box
class DlgMain
{
  HWND hwnd;
  
public:
  DlgMain(HWND hwnd_)
    : hwnd(hwnd_)
  {
  }
  
  // WM_INITDIALOG
  BOOL wmInitDialog(HWND /* focus */, LPARAM /* lParam */)
  {
    setSmallIcon(hwnd, IDI_ICON_mayu);
    setBigIcon(hwnd, IDI_ICON_mayu);
    Edit_SetText(GetDlgItem(hwnd, IDC_EDIT_path), dir.c_str());
    string keyboard1 = loadString(IDS_keyboard1);
    string keyboard2 = loadString(IDS_keyboard2);
    HWND hwndCombo = GetDlgItem(hwnd, IDC_COMBO_keyboard);
    ComboBox_AddString(hwndCombo, keyboard1.c_str());
    ComboBox_AddString(hwndCombo, keyboard2.c_str());
    ComboBox_SetCurSel(hwndCombo, 0);
    string note = loadString(IDS_note01);
    note += loadString(IDS_note02);
    note += loadString(IDS_note03);
    note += loadString(IDS_note04);
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
	bi.lpszTitle      = getString(IDS_selectDir);
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
	  dir = pathFix(buf);
	  doRegisterToStartMenu =
	    (IsDlgButtonChecked(hwnd, IDC_CHECK_registerStartMenu) ==
	     BST_CHECKED);
	  doRegisterToStartUp =
	    (IsDlgButtonChecked(hwnd, IDC_CHECK_registerStartUp) ==
	     BST_CHECKED);
	  switch (ComboBox_GetCurSel(GetDlgItem(hwnd, IDC_COMBO_keyboard)))
	  {
	    case 0: keyboardKind = keyboard109; break;
	    case 1: keyboardKind = keyboard104; break;
	  };
	  EndDialog(hwnd, 1);
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
};


BOOL CALLBACK dlgProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
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


// uninstall step1
int uninstallStep1()
{
  // copy this EXEcutable image into the user's temp directory
  char setup_exe[GANA_MAX_PATH], tmp_setup_exe[GANA_MAX_PATH];
  GetModuleFileName(NULL, setup_exe, sizeof(setup_exe));
  GetTempPath(sizeof(tmp_setup_exe), tmp_setup_exe);
  GetTempFileName(tmp_setup_exe, "del", 0, tmp_setup_exe);
  CopyFile(setup_exe, tmp_setup_exe, FALSE);
    
  // open the clone EXE using FILE_FLAG_DELETE_ON_CLOSE
  HANDLE hfile = CreateFile(tmp_setup_exe, 0, FILE_SHARE_READ, NULL,
			    OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    
  // spawn the clone EXE passing it our EXE's process handle
  // and the full path name to the original EXE file.
  char commandLine[512];
  HANDLE hProcessOrig =
    OpenProcess(SYNCHRONIZE, TRUE, GetCurrentProcessId());
  _snprintf(commandLine, sizeof(commandLine), "%s -u %d",
	    tmp_setup_exe, hProcessOrig);
  STARTUPINFO si;
  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi;
  CreateProcess(NULL, commandLine, NULL, NULL, TRUE, 0, NULL, NULL, &si,&pi);
  Sleep(2000); // important
  CloseHandle(hProcessOrig);
  CloseHandle(hfile);
    
  return 0;
}
  

// uninstall step2
// (in this function, we cannot use any resource, so we use strres[])
int uninstallStep2()
{
  // clone EXE: When original EXE terminates, delete it
  HANDLE hProcessOrig = (HANDLE)atoi(__argv[2]);
  WaitForSingleObject(hProcessOrig, INFINITE);
  CloseHandle(hProcessOrig);
    
  if (IDYES != message(IDS_removeOk, MB_YESNO | MB_ICONQUESTION))
    return 1;

  if (!removeService())
    return 1;
    
  removeFiles();
  removeMayuFileExtenson();
  removeUninstallInformation();
  RemoveDirectory(dir.c_str());
  message(IDS_removeFinish, MB_OK | MB_ICONINFORMATION);

  return 0;
}


// install
int install()
{
  while (true)
  {
    // ask installation directory
    if (!DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG_main), NULL, dlgProc))
      return 1;
      
    // create directory
    if (createDirectories(dir.c_str()))
      break;
    message(IDS_invalidDirectory, MB_OK | MB_ICONSTOP);
  }
    
  _true( Registry::write(DIR_REGISTRY_ROOT, "dir", dir) );
    
  if (!installFiles())
  {
    removeFiles();
    return 1;
  }
  
  createMayuFileExtenson();
  createUninstallInformation();
    
  if (!createService())
  {
    removeFiles();
    removeMayuFileExtenson();
    removeUninstallInformation();
    RemoveDirectory(dir.c_str());
  }
  else
    message(IDS_copyFinish, MB_OK | MB_ICONINFORMATION); // success
  return 0;
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /* hPrevInstance */,
		   LPSTR /* lpszCmdLine */, int /* nCmdShow */)
{
  hInst = hInstance;
  istring program_files;
  Registry::read(PROGRAM_FILES_REGISTRY, &program_files);
  Registry::read(DIR_REGISTRY_ROOT, "dir", &dir, program_files + "\\mayu");

  int retval = 1;
  
  if (__argc == 2 && StringTool::mbsiequal_(__argv[1], "-u"))
    retval = uninstallStep1();
  else
  {
    // is mayu running ?
    HANDLE mutex = CreateMutex(NULL, TRUE, mutexMayuExclusiveRunning);
    if (GetLastError() == ERROR_ALREADY_EXISTS) // mayu is running
      message(IDS_mayuRunning, MB_OK | MB_ICONSTOP);
    else if (__argc == 3 && StringTool::mbsiequal_(__argv[1], "-u"))
      retval = uninstallStep2();
    else if (__argc == 1)
      retval = install();
    CloseHandle(mutex);
  }
  
  return retval;
}
