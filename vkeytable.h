// ////////////////////////////////////////////////////////////////////////////
// vkeytable.h


#ifndef _VKEYTABLE_H
#  define _VKEYTABLE_H

#  include <tchar.h>


///
class VKeyTable
{
public:
  unsigned char m_code;				///
  const _TCHAR *m_name;				///
};

extern const VKeyTable g_vkeyTable[];		///


#endif // _VKEYTABLE_H
