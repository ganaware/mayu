///////////////////////////////////////////////////////////////////////////////
// registry.cpp


#include "registry.h"
#include "stringtool.h"

#include <malloc.h>


using namespace std;


// remove
bool Registry::remove(HKEY root, const string &path, const string &name)
{
  if (name.empty())
    return RegDeleteKey(root, path.c_str()) == ERROR_SUCCESS;
  HKEY hkey;
  if (ERROR_SUCCESS !=
      RegOpenKeyEx(root, path.c_str(), 0, KEY_SET_VALUE, &hkey))
    return false;
  LONG r = RegDeleteValue(hkey, name.c_str());
  RegCloseKey(hkey);
  return r == ERROR_SUCCESS;
}


// does exist the key ?
bool Registry::doesExist(HKEY root, const string &path)
{
  HKEY hkey;
  if (ERROR_SUCCESS != RegOpenKeyEx(root, path.c_str(), 0, KEY_READ, &hkey))
    return false;
  RegCloseKey(hkey);
  return true;
}


// read DWORD
bool Registry::read(HKEY root, const string &path,
		    const string &name, int *value_r, int default_value)
{
  HKEY hkey;
  if (ERROR_SUCCESS == RegOpenKeyEx(root, path.c_str(), 0, KEY_READ, &hkey))
  {
    DWORD type = REG_DWORD;
    DWORD size = sizeof(*value_r);
    LONG r = RegQueryValueEx(hkey, name.c_str(), NULL,
			     &type, (BYTE *)value_r, &size);
    RegCloseKey(hkey);
    if (r == ERROR_SUCCESS)
      return true;
  }
  *value_r = default_value;
  return false;
}


// write DWORD
bool Registry::write(HKEY root, const string &path, const string &name,
		     int value)
{
  HKEY hkey;
  DWORD disposition;
  if (ERROR_SUCCESS !=
      RegCreateKeyEx(root, path.c_str(), 0, "REG_SZ", REG_OPTION_NON_VOLATILE,
		     KEY_ALL_ACCESS, NULL, &hkey, &disposition))
    return false;
  LONG r = RegSetValueEx(hkey, name.c_str(), NULL, REG_DWORD,
			 (BYTE *)&value, sizeof(value));
  RegCloseKey(hkey);
  return r == ERROR_SUCCESS;
}


// read string
bool Registry::read(HKEY root, const string &path, const string &name,
		    string *value_r, const string &default_value)
{
  HKEY hkey;
  if (ERROR_SUCCESS == RegOpenKeyEx(root, path.c_str(), 0, KEY_READ, &hkey))
  {
    DWORD type = REG_SZ;
    DWORD size = 0;
    BYTE dummy;
    if (ERROR_MORE_DATA ==
	RegQueryValueEx(hkey, name.c_str(), NULL, &type, &dummy, &size))
    {
      BYTE *buf = (BYTE *)_alloca(size);
      if (ERROR_SUCCESS ==
	  RegQueryValueEx(hkey, name.c_str(), NULL, &type, buf, &size))
      {
	*value_r = (char *)buf;
	RegCloseKey(hkey);
	return true;
      }
    }
    RegCloseKey(hkey);
  }
  if (!default_value.empty())
    *value_r = default_value;
  return false;
}


// write string
bool Registry::write(HKEY root, const string &path,
		     const string &name, const string &value)
{
  HKEY hkey;
  DWORD disposition;
  if (ERROR_SUCCESS !=
      RegCreateKeyEx(root, path.c_str(), 0, "REG_SZ", REG_OPTION_NON_VOLATILE,
		     KEY_ALL_ACCESS, NULL, &hkey, &disposition))
    return false;
  RegSetValueEx(hkey, name.c_str(), NULL, REG_SZ,
		(BYTE *)value.c_str(), value.size() + 1);
  RegCloseKey(hkey);
  return true;
}


// read binary
bool Registry::read(HKEY root, const string &path,
		    const string &name, BYTE *value_r, DWORD sizeof_value_r,
		    const BYTE *default_value, DWORD sizeof_default_value)
{
  if (value_r && 0 < sizeof_value_r)
  {
    HKEY hkey;
    if (ERROR_SUCCESS == RegOpenKeyEx(root, path.c_str(), 0, KEY_READ, &hkey))
    {
      DWORD type = REG_BINARY;
      LONG r = RegQueryValueEx(hkey, name.c_str(), NULL, &type,
			       (BYTE *)value_r, &sizeof_value_r);
      RegCloseKey(hkey);
      if (r == ERROR_SUCCESS)
	return true;
    }
  }
  if (default_value)
    CopyMemory(value_r, default_value,
	       min(sizeof_default_value, sizeof_value_r));
  return false;
}


// write binary
bool Registry::write(HKEY root, const string &path, const string &name,
		     const BYTE *value, DWORD sizeof_value)
{
  if (!value)
    return false;
  HKEY hkey;
  DWORD disposition;
  if (ERROR_SUCCESS !=
      RegCreateKeyEx(root, path.c_str(), 0, "REG_BINARY",
		     REG_OPTION_NON_VOLATILE,
		     KEY_ALL_ACCESS, NULL, &hkey, &disposition))
    return false;
  RegSetValueEx(hkey, name.c_str(), NULL, REG_BINARY, value, sizeof_value);
  RegCloseKey(hkey);
  return true;
}


static bool string2logfont(LOGFONT *lf, const string &strlf)
{
  char *p = (char *)_alloca(strlf.size() + 1);
  strcpy(p, strlf.c_str());
  
  if (!(p = StringTool::mbstok_(p   , ","))) return false;
  lf->lfHeight         =       atoi(p);
  if (!(p = StringTool::mbstok_(NULL, ","))) return false;
  lf->lfWidth          =       atoi(p);
  if (!(p = StringTool::mbstok_(NULL, ","))) return false;
  lf->lfEscapement     =       atoi(p);
  if (!(p = StringTool::mbstok_(NULL, ","))) return false;
  lf->lfOrientation    =       atoi(p);
  if (!(p = StringTool::mbstok_(NULL, ","))) return false;
  lf->lfWeight         =       atoi(p);
  if (!(p = StringTool::mbstok_(NULL, ","))) return false;
  lf->lfItalic         = (BYTE)atoi(p);
  if (!(p = StringTool::mbstok_(NULL, ","))) return false;
  lf->lfUnderline      = (BYTE)atoi(p);
  if (!(p = StringTool::mbstok_(NULL, ","))) return false;
  lf->lfStrikeOut      = (BYTE)atoi(p);
  if (!(p = StringTool::mbstok_(NULL, ","))) return false;
  lf->lfCharSet        = (BYTE)atoi(p);
  if (!(p = StringTool::mbstok_(NULL, ","))) return false;
  lf->lfOutPrecision   = (BYTE)atoi(p);
  if (!(p = StringTool::mbstok_(NULL, ","))) return false;
  lf->lfClipPrecision  = (BYTE)atoi(p);
  if (!(p = StringTool::mbstok_(NULL, ","))) return false;
  lf->lfQuality        = (BYTE)atoi(p);
  if (!(p = StringTool::mbstok_(NULL, ","))) return false;
  lf->lfPitchAndFamily = (BYTE)atoi(p);
  if (!(p = StringTool::mbstok_(NULL, ","))) return false;
  strncpy(lf->lfFaceName, p, sizeof(lf->lfFaceName));
  lf->lfFaceName[lengthof(lf->lfFaceName) - 1] = '\0';
  return true;
}


// read LOGFONT
bool Registry::read(HKEY root, const string &path, const string &name,
		    LOGFONT *value_r, const string &default_string_value)
{
  string buf;
  if (!read(root, path, name, &buf) || !string2logfont(value_r, buf))
  {
    if (!default_string_value.empty())
      string2logfont(value_r, default_string_value);
    return false;
  }
  return true;
}


// write LOGFONT
bool Registry::write(HKEY root, const string &path, const string &name,
		     const LOGFONT &value)
{
  char buf[1024];
  _snprintf(buf, lengthof(buf), "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s",
	    value.lfHeight, value.lfWidth, value.lfEscapement,
	    value.lfOrientation, value.lfWeight, value.lfItalic,
	    value.lfUnderline, value.lfStrikeOut, value.lfCharSet,
	    value.lfOutPrecision, value.lfClipPrecision, value.lfQuality,
	    value.lfPitchAndFamily, value.lfFaceName);
  return Registry::write(root, path, name, buf);
}
