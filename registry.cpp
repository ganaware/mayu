// ////////////////////////////////////////////////////////////////////////////
// registry.cpp


#include "registry.h"
#include "stringtool.h"

#include <malloc.h>


using namespace std;


// remove
bool Registry::remove(HKEY i_root, const string &i_path, const string &i_name)
{
  if (i_name.empty())
    return RegDeleteKey(i_root, i_path.c_str()) == ERROR_SUCCESS;
  HKEY hkey;
  if (ERROR_SUCCESS !=
      RegOpenKeyEx(i_root, i_path.c_str(), 0, KEY_SET_VALUE, &hkey))
    return false;
  LONG r = RegDeleteValue(hkey, i_name.c_str());
  RegCloseKey(hkey);
  return r == ERROR_SUCCESS;
}


// does exist the key ?
bool Registry::doesExist(HKEY i_root, const string &i_path)
{
  HKEY hkey;
  if (ERROR_SUCCESS !=
      RegOpenKeyEx(i_root, i_path.c_str(), 0, KEY_READ, &hkey))
    return false;
  RegCloseKey(hkey);
  return true;
}


// read DWORD
bool Registry::read(HKEY i_root, const string &i_path,
		    const string &i_name, int *o_value, int i_defaultValue)
{
  HKEY hkey;
  if (ERROR_SUCCESS ==
      RegOpenKeyEx(i_root, i_path.c_str(), 0, KEY_READ, &hkey))
  {
    DWORD type = REG_DWORD;
    DWORD size = sizeof(*o_value);
    LONG r = RegQueryValueEx(hkey, i_name.c_str(), NULL,
			     &type, (BYTE *)o_value, &size);
    RegCloseKey(hkey);
    if (r == ERROR_SUCCESS)
      return true;
  }
  *o_value = i_defaultValue;
  return false;
}


// write DWORD
bool Registry::write(HKEY i_root, const string &i_path, const string &i_name,
		     int i_value)
{
  HKEY hkey;
  DWORD disposition;
  if (ERROR_SUCCESS !=
      RegCreateKeyEx(i_root, i_path.c_str(), 0, "REG_SZ",
		     REG_OPTION_NON_VOLATILE,
		     KEY_ALL_ACCESS, NULL, &hkey, &disposition))
    return false;
  LONG r = RegSetValueEx(hkey, i_name.c_str(), NULL, REG_DWORD,
			 (BYTE *)&i_value, sizeof(i_value));
  RegCloseKey(hkey);
  return r == ERROR_SUCCESS;
}


// read string
bool Registry::read(HKEY i_root, const string &i_path, const string &i_name,
		    string *o_value, const string &i_defaultValue)
{
  HKEY hkey;
  if (ERROR_SUCCESS ==
      RegOpenKeyEx(i_root, i_path.c_str(), 0, KEY_READ, &hkey))
  {
    DWORD type = REG_SZ;
    DWORD size = 0;
    BYTE dummy;
    if (ERROR_MORE_DATA ==
	RegQueryValueEx(hkey, i_name.c_str(), NULL, &type, &dummy, &size))
    {
      BYTE *buf = (BYTE *)_alloca(size);
      if (ERROR_SUCCESS ==
	  RegQueryValueEx(hkey, i_name.c_str(), NULL, &type, buf, &size))
      {
	*o_value = (char *)buf;
	RegCloseKey(hkey);
	return true;
      }
    }
    RegCloseKey(hkey);
  }
  if (!i_defaultValue.empty())
    *o_value = i_defaultValue;
  return false;
}


// write string
bool Registry::write(HKEY i_root, const string &i_path,
		     const string &i_name, const string &i_value)
{
  HKEY hkey;
  DWORD disposition;
  if (ERROR_SUCCESS !=
      RegCreateKeyEx(i_root, i_path.c_str(), 0, "REG_SZ",
		     REG_OPTION_NON_VOLATILE,
		     KEY_ALL_ACCESS, NULL, &hkey, &disposition))
    return false;
  RegSetValueEx(hkey, i_name.c_str(), NULL, REG_SZ,
		(BYTE *)i_value.c_str(), i_value.size() + 1);
  RegCloseKey(hkey);
  return true;
}


// read binary
bool Registry::read(HKEY i_root, const string &i_path,
		    const string &i_name, BYTE *o_value, DWORD i_valueSize,
		    const BYTE *i_defaultValue, DWORD i_defaultValueSize)
{
  if (o_value && 0 < i_valueSize)
  {
    HKEY hkey;
    if (ERROR_SUCCESS ==
	RegOpenKeyEx(i_root, i_path.c_str(), 0, KEY_READ, &hkey))
    {
      DWORD type = REG_BINARY;
      LONG r = RegQueryValueEx(hkey, i_name.c_str(), NULL, &type,
			       (BYTE *)o_value, &i_valueSize);
      RegCloseKey(hkey);
      if (r == ERROR_SUCCESS)
	return true;
    }
  }
  if (i_defaultValue)
    CopyMemory(o_value, i_defaultValue,
	       min(i_defaultValueSize, i_valueSize));
  return false;
}


// write binary
bool Registry::write(HKEY i_root, const string &i_path, const string &i_name,
		     const BYTE *i_value, DWORD i_valueSize)
{
  if (!i_value)
    return false;
  HKEY hkey;
  DWORD disposition;
  if (ERROR_SUCCESS !=
      RegCreateKeyEx(i_root, i_path.c_str(), 0, "REG_BINARY",
		     REG_OPTION_NON_VOLATILE,
		     KEY_ALL_ACCESS, NULL, &hkey, &disposition))
    return false;
  RegSetValueEx(hkey, i_name.c_str(), NULL, REG_BINARY, i_value, i_valueSize);
  RegCloseKey(hkey);
  return true;
}


///
static bool string2logfont(LOGFONT *o_lf, const string &i_strlf)
{
  char *p = (char *)_alloca(i_strlf.size() + 1);
  strcpy(p, i_strlf.c_str());
  
  if (!(p = StringTool::mbstok_(p   , ","))) return false;
  o_lf->lfHeight         =       atoi(p);
  if (!(p = StringTool::mbstok_(NULL, ","))) return false;
  o_lf->lfWidth          =       atoi(p);
  if (!(p = StringTool::mbstok_(NULL, ","))) return false;
  o_lf->lfEscapement     =       atoi(p);
  if (!(p = StringTool::mbstok_(NULL, ","))) return false;
  o_lf->lfOrientation    =       atoi(p);
  if (!(p = StringTool::mbstok_(NULL, ","))) return false;
  o_lf->lfWeight         =       atoi(p);
  if (!(p = StringTool::mbstok_(NULL, ","))) return false;
  o_lf->lfItalic         = (BYTE)atoi(p);
  if (!(p = StringTool::mbstok_(NULL, ","))) return false;
  o_lf->lfUnderline      = (BYTE)atoi(p);
  if (!(p = StringTool::mbstok_(NULL, ","))) return false;
  o_lf->lfStrikeOut      = (BYTE)atoi(p);
  if (!(p = StringTool::mbstok_(NULL, ","))) return false;
  o_lf->lfCharSet        = (BYTE)atoi(p);
  if (!(p = StringTool::mbstok_(NULL, ","))) return false;
  o_lf->lfOutPrecision   = (BYTE)atoi(p);
  if (!(p = StringTool::mbstok_(NULL, ","))) return false;
  o_lf->lfClipPrecision  = (BYTE)atoi(p);
  if (!(p = StringTool::mbstok_(NULL, ","))) return false;
  o_lf->lfQuality        = (BYTE)atoi(p);
  if (!(p = StringTool::mbstok_(NULL, ","))) return false;
  o_lf->lfPitchAndFamily = (BYTE)atoi(p);
  if (!(p = StringTool::mbstok_(NULL, ","))) return false;
  strncpy(o_lf->lfFaceName, p, sizeof(o_lf->lfFaceName));
  o_lf->lfFaceName[NUMBER_OF(o_lf->lfFaceName) - 1] = '\0';
  return true;
}


// read LOGFONT
bool Registry::read(HKEY i_root, const string &i_path, const string &i_name,
		    LOGFONT *o_value, const string &i_defaultStringValue)
{
  string buf;
  if (!read(i_root, i_path, i_name, &buf) || !string2logfont(o_value, buf))
  {
    if (!i_defaultStringValue.empty())
      string2logfont(o_value, i_defaultStringValue);
    return false;
  }
  return true;
}


// write LOGFONT
bool Registry::write(HKEY i_root, const string &i_path, const string &i_name,
		     const LOGFONT &i_value)
{
  char buf[1024];
  _snprintf(buf, NUMBER_OF(buf), "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s",
	    i_value.lfHeight, i_value.lfWidth, i_value.lfEscapement,
	    i_value.lfOrientation, i_value.lfWeight, i_value.lfItalic,
	    i_value.lfUnderline, i_value.lfStrikeOut, i_value.lfCharSet,
	    i_value.lfOutPrecision, i_value.lfClipPrecision, i_value.lfQuality,
	    i_value.lfPitchAndFamily, i_value.lfFaceName);
  return Registry::write(i_root, i_path, i_name, buf);
}
