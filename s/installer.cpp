///////////////////////////////////////////////////////////////////////////////
// setup.cpp


#include "../misc.h"
#include "../registry.h"
#include "../stringtool.h"
#include "../windowstool.h"
#include "../regexp.h"
#include "installer.h"

#include <shlobj.h>
#include <sys/types.h>
#include <sys/stat.h>


namespace Installer
{
  using namespace std;
  using namespace StringTool;
  
  
  /////////////////////////////////////////////////////////////////////////////
  // Utility Functions

  // createLink - uses the shell's IShellLink and IPersistFile interfaces 
  //   to create and store a shortcut to the specified object. 
  // Returns the result of calling the member functions of the interfaces. 
  // lpszPathObj - address of a buffer containing the path of the object. 
  // lpszPathLink - address of a buffer containing the path where the 
  //   shell link is to be stored. 
  // lpszDesc - address of a buffer containing the description of the 
  //   shell link. 
  HRESULT createLink(LPCSTR lpszPathObj, LPCSTR lpszPathLink, LPCSTR lpszDesc)
  { 
    HRESULT hres;
    IShellLink* psl;

    // Get a pointer to the IShellLink interface. 
    hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
			    IID_IShellLink, (void **)&psl);
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


  // create file extension information
  void createFileExtension(const istring &ext, const string &contentType,
			   const istring &fileType, const string &fileTypeName,
			   const istring &iconPath, const string &command)
  {
    string dummy;

    Registry regExt(HKEY_CLASSES_ROOT, ext);
    if (!    regExt.read ("", &dummy))
      _true( regExt.write("", fileType) );
    if (!    regExt.read ("Content Type", &dummy))
      _true( regExt.write("Content Type", contentType) );

    Registry regFileType(HKEY_CLASSES_ROOT, fileType);
    if (!    regFileType.read ("", &dummy))
      _true( regFileType.write("", fileTypeName) );

    Registry regFileTypeIcon(HKEY_CLASSES_ROOT, fileType + "\\DefaultIcon");
    if (!    regFileTypeIcon.read ("", &dummy))
      _true( regFileTypeIcon.write("", iconPath) );

    Registry regFileTypeComand(HKEY_CLASSES_ROOT,
			       fileType + "\\shell\\open\\command");
    if (!    regFileTypeComand.read ("", &dummy))
      _true( regFileTypeComand.write("", command) );
  }


  // remove file extension information
  void removeFileExtension(const istring &ext, const istring &fileType)
  {
    Registry::remove(HKEY_CLASSES_ROOT, ext);
    Registry::remove(HKEY_CLASSES_ROOT, fileType + "\\shell\\open\\command");
    Registry::remove(HKEY_CLASSES_ROOT, fileType + "\\shell\\open");
    Registry::remove(HKEY_CLASSES_ROOT, fileType + "\\shell");
    Registry::remove(HKEY_CLASSES_ROOT, fileType);
  }

  
  // create uninstallation information
  void createUninstallInformation(const istring &name,
				  const string &displayName,
				  const string &commandLine)
  {
    Registry reg(HKEY_LOCAL_MACHINE,
		 "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
		 + name);

    _true( reg.write("DisplayName", displayName) );
    _true( reg.write("UninstallString", commandLine) );
  }

  
  // remove uninstallation information
  void removeUninstallInformation(const istring &name)
  {
    Registry::
      remove(HKEY_LOCAL_MACHINE,
	     "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
	     + name);
  }
  
  
  // normalize path
  istring normalizePath(istring path)
  {
    Regexp regSlash("^(.*)/(.*)$");
    while (regSlash.doesMatch(path))
      path = regSlash[1] + "\\" + regSlash[2];

    Regexp regTailBackSlash("^(.*)\\\\$");
    if (regTailBackSlash.doesMatch(path))
      path = regTailBackSlash[1];
  
    return path;
  }

  
  // create deep directory
  bool createDirectories(const char *folder)
  {
    const char *s = strchr(folder, '\\'); // TODO: '/'
    if (s && s - folder == 2 && folder[1] == ':')
      s = strchr(s + 1, '\\');
    
    struct stat sbuf;
    while (s)
    {
      istring f(folder, 0, s - folder);
      if (stat(f.c_str(), &sbuf) < 0)
	if (!CreateDirectory(f.c_str(), NULL))
	  return false;
      s = strchr(s + 1, '\\');
    }
    if (stat(folder, &sbuf) < 0)
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
  istring getModuleDirectory()
  {
    char buf[GANA_MAX_PATH];
    _true( GetModuleFileName(hInst, buf, sizeof(buf)) );
    Regexp reg("^(.*)\\\\[^\\\\]*$");
    if (reg.doesMatch(buf))
      return reg[1];
    else
      return buf;
  }


  // get start menu name
  istring getStartMenuName(const istring &shortcutName)
  {
#if 0
    char programDir[GANA_MAX_PATH];
    if (SUCCEEDED(SHGetSpecialFolderPath(NULL, programDir,
					 CSIDL_COMMON_PROGRAMS, FALSE)))
      return istring(programDir) + "\\" + shortcutName + ".lnk";
#else
    istring programDir;
    if (Registry::read(HKEY_LOCAL_MACHINE,
		       "Software\\Microsoft\\Windows\\CurrentVersion\\"
		       "Explorer\\Shell Folders", "Common Programs",
		       &programDir))
      return programDir + "\\" + shortcutName + ".lnk";
#endif
    return "";
  }


  // get start up name
  istring getStartUpName(const istring &shortcutName)
  {
    istring startupDir;
    if (Registry::read(HKEY_CURRENT_USER,
		       "Software\\Microsoft\\Windows\\CurrentVersion\\"
		       "Explorer\\Shell Folders", "Startup", &startupDir))
      return startupDir + "\\" + shortcutName + ".lnk";
    return "";
  }


  // create driver service
  DWORD createDriverService(const istring &serviceName,
			    const string &serviceDescription,
			    const istring &driverPath,
			    const char *preloadedGroups)
  {
    SC_HANDLE hscm =
      OpenSCManager(NULL, NULL,
		    SC_MANAGER_CREATE_SERVICE | SC_MANAGER_CONNECT);
    if (!hscm)
      return false;

    SC_HANDLE hs =
      CreateService(hscm, serviceName.c_str(), serviceDescription.c_str(),
		    SERVICE_START | SERVICE_STOP, SERVICE_KERNEL_DRIVER,
		    SERVICE_AUTO_START, SERVICE_ERROR_IGNORE,
		    driverPath.c_str(), NULL, NULL,
		    preloadedGroups, NULL, NULL);
    if (hs == NULL)
    {
      DWORD err = GetLastError();
      switch (err)
      {
	case ERROR_SERVICE_EXISTS:
	  break;
	default:
	  return err;
      }
    }
    CloseServiceHandle(hs);
    CloseServiceHandle(hscm);
    return ERROR_SUCCESS;
  }


  // remove driver service
  DWORD removeDriverService(const istring &serviceName)
  {
    DWORD err = ERROR_SUCCESS;
    
    SC_HANDLE hscm = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    SC_HANDLE hs =
      OpenService(hscm, serviceName.c_str(),
		  SERVICE_START | SERVICE_STOP | DELETE);
    if (!hs)
    {
      err = GetLastError();
      goto error;
    }

    SERVICE_STATUS ss;
    ControlService(hs, SERVICE_CONTROL_STOP, &ss);
  
    if (!DeleteService(hs))
    {
      err = GetLastError();
      goto error;
    }
    error:
    CloseServiceHandle(hs);
    CloseServiceHandle(hscm);
    return err;
  }


  // check operating system
  bool checkOs(SetupFile::OS os)
  {
    OSVERSIONINFO ver;
    ver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&ver);
    
    switch (os)
    {
      default:
      case SetupFile::ALL:
	return true;
      case SetupFile::W9x:
	return (ver.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS &&
		4 <= ver.dwMajorVersion);
      case SetupFile::NT :
	return (ver.dwPlatformId == VER_PLATFORM_WIN32_NT &&
		4 <= ver.dwMajorVersion);
      case SetupFile::NT4:
	return (ver.dwPlatformId == VER_PLATFORM_WIN32_NT &&
		ver.dwMajorVersion == 4);
      case SetupFile::W2k:
	return (ver.dwPlatformId == VER_PLATFORM_WIN32_NT &&
		5 <= ver.dwMajorVersion);
    }
  }

  
  // install files
  bool installFiles(const SetupFile *setupFiles, size_t lengthof_setupFiles,
		    const istring &srcDir, const istring &destDir)
  {
    istring to, from;
    istring destDriverDir = getDriverDirectory();

    for (size_t i = 0; i < lengthof_setupFiles; i ++)
    {
      const SetupFile &s = setupFiles[i];
      const istring &fromDir = srcDir;
      const istring &toDir =
	(s.destination == SetupFile::ToDriver) ? destDriverDir : destDir;

      if (!s.from)
	continue;	// remove only

      if (fromDir == toDir)
	continue;	// same directory

      if (!checkOs(s.os))	// check operating system
	continue;

      // type
      switch (s.kind)
      {
	case SetupFile::Dll:
	{
	  // rename driver
	  istring from_ = toDir + "\\" + s.to;
	  istring to_ = toDir + "\\deleted." + s.to;
	  DeleteFile(to_.c_str());
	  MoveFile(from_.c_str(), to_.c_str());
	  DeleteFile(to_.c_str());
	}
	// fall through
	default:
	case SetupFile::File:
	{
	  from += fromDir + '\\' + s.from + '\0';
	  to   += toDir   + '\\' + s.to   + '\0';
	  break;
	}
	case SetupFile::Dir:
	{
	  createDirectories((toDir + '\\' + s.to).c_str());
	  break;
	}
      }
    }

    SHFILEOPSTRUCT fo;
    ZeroMemory(&fo, sizeof(fo));
    fo.wFunc = FO_COPY;
    fo.fFlags = FOF_MULTIDESTFILES;
    fo.pFrom = from.c_str();
    fo.pTo   = to.c_str();
    if (SHFileOperation(&fo) || fo.fAnyOperationsAborted)
      return false;
    return true;
  }

  // remove files from src
  bool removeSrcFiles(const SetupFile *setupFiles, size_t lengthof_setupFiles,
		      const istring &srcDir)
  {
    istring destDriverDir = getDriverDirectory();

    for (size_t i = 0; i < lengthof_setupFiles; i ++)
    {
      const SetupFile &s = setupFiles[lengthof_setupFiles - i - 1];
      const istring &fromDir = srcDir;
      
      if (!s.from)
	continue;	// remove only

      if (!checkOs(s.os))	// check operating system
	continue;

      // type
      switch (s.kind)
      {
	default:
	case SetupFile::Dll:
	case SetupFile::File:
	  DeleteFile((fromDir + '\\' + s.from).c_str());
	  break;
	case SetupFile::Dir:
	  RemoveDirectory((fromDir + '\\' + s.from).c_str());
	  break;
      }
    }
    RemoveDirectory(srcDir.c_str());
    return true;
  }

  
  // remove files
  void removeFiles(const SetupFile *setupFiles, size_t lengthof_setupFiles,
		   const istring &destDir)
  {
    istring destDriverDir = getDriverDirectory();

    for (size_t i = 0; i < lengthof_setupFiles; i ++)
    {
      const SetupFile &s = setupFiles[lengthof_setupFiles - i - 1];
      const istring &toDir =
	(s.destination == SetupFile::ToDriver) ? destDriverDir : destDir;

      if (!checkOs(s.os))	// check operating system
	continue;

      // type
      switch (s.kind)
      {
	case SetupFile::Dll:
	  DeleteFile((toDir + "\\deleted." + s.to).c_str());
	  // fall through
	default:
	case SetupFile::File:
	  DeleteFile((toDir + '\\' + s.to).c_str());
	  break;
	case SetupFile::Dir:
	  RemoveDirectory((toDir + '\\' + s.to).c_str());
	  break;
      }
    }
    RemoveDirectory(destDir.c_str());
  }
  
  
  // uninstall step1
  int uninstallStep1(const char *uninstallOption)
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
    _snprintf(commandLine, sizeof(commandLine), "%s %s %d",
	      tmp_setup_exe, uninstallOption, hProcessOrig);
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
  // (after this function, we cannot use any resource)
  void uninstallStep2(const char *argByStep1)
  {
    // clone EXE: When original EXE terminates, delete it
    HANDLE hProcessOrig = (HANDLE)atoi(argByStep1);
    WaitForSingleObject(hProcessOrig, INFINITE);
    CloseHandle(hProcessOrig);
  }
  
  
  /////////////////////////////////////////////////////////////////////////////
  // Locale / StringResource


  // constructor
  Resource::Resource(const StringResource *stringResources_)
    : stringResources(stringResources_),
      locale(Locale_C)
  {
    struct LocaleInformaton
    {
      const char *localeString;
      Locale locale;
    };

    // set locale information
    const char *localeString = setlocale(LC_ALL, "");
    
    static const LocaleInformaton locales[] =
    {
      { "Japanese_Japan.932", Locale_Japanese_Japan_932 },
    };

    for (size_t i = 0; i < lengthof(locales); i ++)
      if (mbsiequal_(localeString, locales[i].localeString))
      {
	locale = locales[i].locale;
	break;
      }
  }
  
  
  // get resource string
  const char *Resource::loadString(UINT id)
  {
    int n = (int)locale;
    int index = -1;
    for (int i = 0; stringResources[i].str; i ++)
      if (stringResources[i].id == id)
      {
	if (n == 0)
	  return stringResources[i].str;
	index = i;
	n --;
      }
    if (0 <= index)
      return stringResources[index].str;
    else
      return "";
  }
}
