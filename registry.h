// ////////////////////////////////////////////////////////////////////////////
// registry.h

#ifndef _REGISTRY_H
#  define _REGISTRY_H


#  include <string>
#  include <windows.h>


///
class Registry
{
  HKEY m_root;					/// registry root
  std::string m_path;				/// path from registry root
  
public:
  ///
  Registry() : m_root(NULL) { }
  ///
  Registry(HKEY i_root, const std::string &i_path)
    : m_root(i_root), m_path(i_path) { }
  
  /// set registry root and path
  void setRoot(HKEY i_root, const std::string &i_path)
  { m_root = i_root; m_path = i_path; }
  
  /// remvoe
  bool remove(const std::string &i_name = "") const
  { return remove(m_root, m_path, i_name); }
  
  /// does exist the key ?
  bool doesExist() const { return doesExist(m_root, m_path); }

  /// DWORD
  bool read(const std::string &i_name, int *o_value, int i_defaultValue = 0)
    const
  { return read(m_root, m_path, i_name, o_value, i_defaultValue); }
  ///
  bool write(const std::string &i_name, int i_value) const
  { return write(m_root, m_path, i_name, i_value); }
 
  /// std::string
  bool read(const std::string &i_name, std::string *o_value, 
	    const std::string &i_defaultValue = "") const
  { return read(m_root, m_path, i_name, o_value, i_defaultValue); }
  ///
  bool write(const std::string &i_name, const std::string &i_value) const
  { return write(m_root, m_path, i_name, i_value); }

  /// binary
  bool read(const std::string &i_name, BYTE *o_value, DWORD i_valueSize,
	    const BYTE *i_defaultValue = NULL, DWORD i_defaultValueSize = 0)
    const
  { return read(m_root, m_path, i_name, o_value, i_valueSize, i_defaultValue,
		i_defaultValueSize); }
  ///
  bool write(const std::string &i_name, const BYTE *i_value,
	     DWORD i_valueSize) const
  { return write(m_root, m_path, i_name, i_value, i_valueSize); }

public:
  
  ///
#define Registry_path \
  HKEY i_root, const std::string &i_path, const std::string &i_name
  
  /// remove
  static bool remove(Registry_path = "");
  
  /// does exist the key ?
  static bool doesExist(HKEY i_root, const std::string &i_path);
  
  /// DWORD
  static bool read(Registry_path, int *o_value, int i_defaultValue = 0);
  ///
  static bool write(Registry_path, int i_value);

  /// std::string
  static bool read(Registry_path, std::string *o_value,
		   const std::string &i_defaultValue = "");
  ///
  static bool write(Registry_path, const std::string &i_value);
  
  /// binary
  static bool read(Registry_path, BYTE *o_value, DWORD i_valueSize,
		   const BYTE *i_defaultValue = NULL,
		   DWORD i_defaultValueSize = 0);
  ///
  static bool write(Registry_path, const BYTE *i_value, DWORD i_valueSize);
  /// read LOGFONT
  static bool read(Registry_path, LOGFONT *o_value,
		   const std::string &i_defaultStringValue);
  /// write LOGFONT
  static bool write(Registry_path, const LOGFONT &i_value);
#undef Registry_path
};


#endif // _REGISTRY_H
