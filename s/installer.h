///////////////////////////////////////////////////////////////////////////////
// installer.h


#ifndef __installer_h__
#define __installer_h__


namespace Installer
{
  using namespace std;
  using namespace StringTool;
  
  
  /////////////////////////////////////////////////////////////////////////////
  // SetupFile
  
  // files to copy
  struct SetupFile
  {
  public:
    enum Kind { File, Dir, Dll };
    enum OS
    {
      ALL,
      W9x, // W95, W98,
      NT,  NT4, W2k,
    };
    enum Destination { ToDest, ToDriver };
  
  public:
    const Kind kind;
    const OS os;
    const char *from;
    const Destination destination;
    const char *to;
  
  public:
    SetupFile(Kind kind_, OS os_, const char *from_, Destination destination_,
	      const char *to_ = NULL)
      : kind(kind_),
	os(os_),
	from(from_),
	destination(destination_),
	to(to_ ? to_ : from_)
    {
    }
  };

  
  /////////////////////////////////////////////////////////////////////////////
  // Locale / StringResource
  
  enum Locale
  {
    Locale_Japanese_Japan_932 = 0,
    Locale_C = 1,
  };

  struct StringResource
  {
    UINT id;
    char *str;
  };

  class Resource
  {
    const StringResource *stringResources;
    
    Locale locale;
    
  public:
    // constructor
    Resource(const StringResource *stringResources_);
    Resource(const StringResource *stringResources_, Locale locale_)
      : stringResources(stringResources_), locale(locale_) { }
    
    // get resource string
    const char *loadString(UINT id);

    // locale
    Locale getLocale() const { return locale; }
  };

  
  /////////////////////////////////////////////////////////////////////////////
  // Utility Functions

  // createLink - uses the shell's IShellLink and IPersistFile interfaces 
  //   to create and store a shortcut to the specified object. 
  HRESULT createLink(LPCSTR lpszPathObj, LPCSTR lpszPathLink, LPCSTR lpszDesc);
  
  // create file extension information
  void createFileExtension(const istring &ext, const string &contentType,
			   const istring &fileType, const string &fileTypeName,
			   const istring &iconPath, const string &command);
  
  // remove file extension information
  void removeFileExtension(const istring &ext, const istring &fileType);
  
  // create uninstallation information
  void createUninstallInformation(const istring &name,
				  const string &displayName,
				  const string &commandLine);
  
  // remove uninstallation information
  void removeUninstallInformation(const istring &name);

  // normalize path
  istring normalizePath(istring path);
  
  // create deep directory
  bool createDirectories(const char *folder);

  // get driver directory
  istring getDriverDirectory();

  // get current directory
  istring getModuleDirectory();

  // get start menu name
  istring getStartMenuName(const istring &shortcutName);

  // get start up name
  istring getStartUpName(const istring &shortcutName);

  // create driver service
  DWORD createDriverService(const istring &serviceName,
			    const string &serviceDescription,
			    const istring &driverPath,
			    const char *preloadedGroups);

  // remove driver service
  DWORD removeDriverService(const istring &serviceName);

  // check operating system
  bool checkOs(SetupFile::OS os);
  
  // install files
  bool installFiles(const SetupFile *setupFiles, size_t lengthof_setupFiles,
		    const istring &srcDir, const istring &destDir);
  
  // remove files from src
  bool removeSrcFiles(const SetupFile *setupFiles, size_t lengthof_setupFiles,
		      const istring &srcDir);
  
  // remove files
  void removeFiles(const SetupFile *setupFiles, size_t lengthof_setupFiles,
		   const istring &destDir);
  
  // uninstall step1
  int uninstallStep1(const char *uninstallOption);
  
  // uninstall step2
  // (after this function, we cannot use any resource)
  void uninstallStep2(const char *argByStep1);
}


#endif // __installer_h__
