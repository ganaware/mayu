///////////////////////////////////////////////////////////////////////////////
// driver.h


#ifndef __driver_h__
#define __driver_h__


#include <winioctl.h>


#define MAYU_DEVICE_FILE_NAME "\\\\.\\MayuDetour1"
#define MAYU_DRIVER_NAME "mayud"


// Ioctl value (equivalent to CancelIo API)
#define IOCTL_MAYU_DETOUR_CANCEL					 \
CTL_CODE(FILE_DEVICE_KEYBOARD, 0x0001, METHOD_BUFFERED, FILE_ANY_ACCESS)


struct KEYBOARD_INPUT_DATA
{
  enum { BREAK = 1,			// BREAK is key release
	 E0 = 2, E1 = 4, E0E1 = 6,	// E0, E1 are for extended key,
	 TERMSRV_SET_LED = 8 };
  enum { KEYBOARD_OVERRUN_MAKE_CODE = 0xFF };
  
  USHORT UnitId;	// keyboard unit number
  USHORT MakeCode;	// scan code
  USHORT Flags;		//
  USHORT Reserved;
  ULONG ExtraInformation;
};


#endif // __driver_h__
