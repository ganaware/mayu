// ////////////////////////////////////////////////////////////////////////////
// vkeytable.h


#ifndef _VKEYTABLE_H
#  define _VKEYTABLE_H


///
class VKeyTable
{
public:
  unsigned char m_code;				///
  const char *m_name;				///
};

extern const VKeyTable g_vkeyTable[];		///


#endif // _VKEYTABLE_H
