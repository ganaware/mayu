///////////////////////////////////////////////////////////////////////////////
// registry.h

#ifndef __registry_h__
#define __registry_h__


#include <string>
#include <windows.h>


class Registry
{
  HKEY root;		// registry root
  std::string path;	// path from registry root
  
public:
  Registry() : root(NULL) { }
  Registry(HKEY root_, const std::string &path_) : root(root_), path(path_) { }
  
  // set registry root and path
  void setRoot(HKEY root_, const std::string &path_)
  { root = root_; path = path_; }
  
  // remvoe
  bool remove(const std::string &name = "") const
  { return remove(root, path, name); }
  
  // does exist the key ?
  bool doesExist() const { return doesExist(root, path); }

  // DWORD
  bool read(const std::string &name, int *value_r, int default_value = 0) const
  { return read(root, path, name, value_r, default_value); }
  bool write(const std::string &name, int value) const
  { return write(root, path, name, value); }
 
  // std::string
  bool read(const std::string &name, std::string *value_r, 
	    const std::string &default_value = "") const
  { return read(root, path, name, value_r, default_value); }
  bool write(const std::string &name, const std::string &value) const
  { return write(root, path, name, value); }

  // binary
  bool read(const std::string &name, BYTE *value_r, DWORD sizeof_value_r,
	    const BYTE *default_value = NULL, DWORD sizeof_default_value = 0)
    const
  { return read(root, path, name, value_r, sizeof_value_r, default_value,
		sizeof_default_value); }
  bool write(const std::string &name, const BYTE *value,
	     DWORD sizeof_value) const
  { return write(root, path, name, value, sizeof_value); }

public:
  
#define Registry_path \
  HKEY root, const std::string &path, const std::string &name
  
  // remove
  static bool remove(Registry_path = "");
  
  // does exist the key ?
  static bool doesExist(HKEY root, const std::string &path);
  
  // DWORD
  static bool read(Registry_path, int *value_r, int default_value = 0);
  static bool write(Registry_path, int value);

  // std::string
  static bool read(Registry_path, std::string *value_r,
		   const std::string &default_value = "");
  static bool write(Registry_path, const std::string &value);
  
  // binary
  static bool read(Registry_path, BYTE *value_r, DWORD sizeof_value_r,
		   const BYTE *default_value = NULL,
		   DWORD sizeof_default_value = 0);
  static bool write(Registry_path,
		    const BYTE *value, DWORD sizeof_value);
  // read LOGFONT
  static bool read(Registry_path, LOGFONT *value,
		   const std::string &default_string_value);
  // write LOGFONT
  static bool write(Registry_path, const LOGFONT &value);
#undef Registry_path
};


#endif // __registry_h__
