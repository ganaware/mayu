// ////////////////////////////////////////////////////////////////////////////
// registry.h

#ifndef _REGISTRY_H
#  define _REGISTRY_H

#  include "stringtool.h"


///
class Registry
{
  HKEY m_root;					/// registry root
  tstring m_path;				/// path from registry root
  
public:
  ///
  Registry() : m_root(NULL) { }
  ///
  Registry(HKEY i_root, const tstring &i_path)
    : m_root(i_root), m_path(i_path) { }
  
  /// set registry root and path
  void setRoot(HKEY i_root, const tstring &i_path)
  { m_root = i_root; m_path = i_path; }
  
  /// remvoe
  bool remove(const tstring &i_name = _T("")) const
  { return remove(m_root, m_path, i_name); }
  
  /// does exist the key ?
  bool doesExist() const { return doesExist(m_root, m_path); }

  /// DWORD
  bool read(const tstring &i_name, int *o_value, int i_defaultValue = 0)
    const
  { return read(m_root, m_path, i_name, o_value, i_defaultValue); }
  ///
  bool write(const tstring &i_name, int i_value) const
  { return write(m_root, m_path, i_name, i_value); }
 
  /// tstring
  bool read(const tstring &i_name, tstring *o_value, 
	    const tstring &i_defaultValue = _T("")) const
  { return read(m_root, m_path, i_name, o_value, i_defaultValue); }
  ///
  bool write(const tstring &i_name, const tstring &i_value) const
  { return write(m_root, m_path, i_name, i_value); }

  /// binary
  bool read(const tstring &i_name, BYTE *o_value, DWORD i_valueSize,
	    const BYTE *i_defaultValue = NULL, DWORD i_defaultValueSize = 0)
    const
  { return read(m_root, m_path, i_name, o_value, i_valueSize, i_defaultValue,
		i_defaultValueSize); }
  ///
  bool write(const tstring &i_name, const BYTE *i_value,
	     DWORD i_valueSize) const
  { return write(m_root, m_path, i_name, i_value, i_valueSize); }

public:
  
  ///
#define Registry_path \
  HKEY i_root, const tstring &i_path, const tstring &i_name
  
  /// remove
  static bool remove(Registry_path = _T(""));
  
  /// does exist the key ?
  static bool doesExist(HKEY i_root, const tstring &i_path);
  
  /// DWORD
  static bool read(Registry_path, int *o_value, int i_defaultValue = 0);
  ///
  static bool write(Registry_path, int i_value);

  /// tstring
  static bool read(Registry_path, tstring *o_value,
		   const tstring &i_defaultValue = _T(""));
  ///
  static bool write(Registry_path, const tstring &i_value);
  
  /// binary
  static bool read(Registry_path, BYTE *o_value, DWORD i_valueSize,
		   const BYTE *i_defaultValue = NULL,
		   DWORD i_defaultValueSize = 0);
  ///
  static bool write(Registry_path, const BYTE *i_value, DWORD i_valueSize);
  /// read LOGFONT
  static bool read(Registry_path, LOGFONT *o_value,
		   const tstring &i_defaultStringValue);
  /// write LOGFONT
  static bool write(Registry_path, const LOGFONT &i_value);
#undef Registry_path
};


#endif // _REGISTRY_H
