#include <windows.h>
#include <process.h>
#include <tchar.h>
#include "../driver.h"
#ifdef STS4MAYU
#include "SynKit.h"
#pragma comment(lib, "SynCom.lib")
#endif /* STS4MAYU */
#ifdef CTS4MAYU
#include "Touchpad.h"
#pragma comment(lib, "TouchPad.lib")
#endif /* CTS4MAYU */

static HINSTANCE s_instance;
static HANDLE s_device;

#ifdef STS4MAYU
static ISynAPI *s_synAPI;
static ISynDevice *s_synDevice;
static HANDLE s_notifyEvent;

static int s_terminated;
static HANDLE s_loopThread;
static unsigned int s_loopThreadId;
#endif /* STS4MAYU */
#ifdef CTS4MAYU
static HTOUCHPAD s_hTP[16];
static int s_devNum;
#endif /* CTS4MAYU */

static USHORT s_touchpadScancode;

static void changeTouch(int i_isBreak)
{
  KEYBOARD_INPUT_DATA kid;
  ULONG len;

  kid.UnitId = 0;
  kid.MakeCode = s_touchpadScancode;
  kid.Flags = i_isBreak;
  kid.Reserved = 0;
  kid.ExtraInformation = 0;

  DeviceIoControl(s_device, IOCTL_MAYU_FORCE_KEYBOARD_INPUT,
		  &kid, sizeof(kid), NULL, 0, &len, NULL);
}

#ifdef STS4MAYU
static unsigned int WINAPI loop(void *dummy)
{
  HRESULT result;
  SynPacket packet;
  int isTouched = 0;

  while (s_terminated == 0) {
    WaitForSingleObject(s_notifyEvent, INFINITE);
    if (s_terminated) {
      break;
    }

    for (;;) {
      long value;

      result = s_synAPI->GetEventParameter(&value);
      if (result != SYN_OK) {
	break;
      }
      if (value == SE_Configuration_Changed) {
	s_synDevice->SetEventNotification(s_notifyEvent);
      }
    }

    for (;;) {
      result = s_synDevice->LoadPacket(packet);
      if (result == SYNE_FAIL) {
	break;
      }

      if (isTouched) {
	if (!(packet.FingerState() & SF_FingerTouch)) {
	  changeTouch(1);
	  isTouched = 0;
	}
      } else {
	if (packet.FingerState() & SF_FingerTouch) {
	  changeTouch(0);
	  isTouched = 1;
	}
      }
    }
  }
  _endthreadex(0);
  return 0;
}
#endif /* STS4MAYU */
#ifdef CTS4MAYU
static void CALLBACK TouchpadFunc(HTOUCHPAD hTP, LPFEEDHDR lpFeedHdr, LPARAM lParam)
{
	LPRAWFEED lpRawFeed;
	static int isTouched = 0;

	lpRawFeed = (LPRAWFEED)(lpFeedHdr + 1);
	if (isTouched) {
	  if (!lpRawFeed->wPressure) {
	    changeTouch(1);
	    isTouched = 0;
	  }
	} else {
	  if (lpRawFeed->wPressure) {
	    changeTouch(0);
	    isTouched = 1;
	  }
	}
	EnableWindowsCursor(hTP, TRUE);
}

static BOOL CALLBACK DevicesFunc(LPGENERICDEVICE device, LPARAM lParam)
{
	HTOUCHPAD hTP = NULL;
	BOOL ret = FALSE;

	s_hTP[s_devNum] = GetPad(device->devicePort);
	CreateCallback(s_hTP[s_devNum], TouchpadFunc,
		       TPF_RAW | TPF_POSTMESSAGE, NULL);
	StartFeed(s_hTP[s_devNum]);
	++s_devNum;
	return TRUE;
}
#endif /* CTS4MAYU */

void WINAPI ts4mayuInit(HANDLE i_device, USHORT i_touchpadScancode)
{
  s_device = i_device;
  s_touchpadScancode = i_touchpadScancode;

#ifdef STS4MAYU
  HRESULT result;
  long hdl;

  s_synAPI = NULL;
  s_synDevice = NULL;
  s_notifyEvent = NULL;

  s_terminated = 0;

  result = SynCreateAPI(&s_synAPI);
  if (result != SYN_OK) {
    goto error_on_init;
  }

  hdl = -1;
  result = s_synAPI->FindDevice(SE_ConnectionAny, SE_DeviceTouchPad, &hdl);
  if (result != SYN_OK) {
    goto error_on_init;
  }

  result = s_synAPI->CreateDevice(hdl, &s_synDevice);
  if (result != SYN_OK) {
    goto error_on_init;
  }

  s_notifyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (s_notifyEvent == NULL) {
    goto error_on_init;
  }

  s_synAPI->SetEventNotification(s_notifyEvent);
  s_synDevice->SetEventNotification(s_notifyEvent);

  s_loopThread =
    (HANDLE)_beginthreadex(NULL, 0, loop, NULL, 0, &s_loopThreadId);
  if (s_loopThread == 0) {
    goto error_on_init;
  }

  return;

error_on_init:
  if (s_notifyEvent) {
    CloseHandle(s_notifyEvent);
  }

  if (s_synDevice) {
    s_synDevice->Release();
  }

  if (s_synAPI) {
    s_synAPI->Release();
  }
#endif /* STS4MAYU */
#ifdef CTS4MAYU
  // enumerate devices
  EnumDevices(DevicesFunc, NULL);
#endif /* CTS4MAYU */
}


void WINAPI ts4mayuTerm()
{
#ifdef STS4MAYU
  s_terminated = 1;

  if (s_loopThread) {
    SetEvent(s_notifyEvent);
    WaitForSingleObject(s_loopThread, INFINITE);
    CloseHandle(s_loopThread);
  }

  if (s_notifyEvent) {
    CloseHandle(s_notifyEvent);
  }

  if (s_synDevice) {
    s_synDevice->Release();
  }

  if (s_synAPI) {
    s_synAPI->Release();
  }
#endif /* STS4MAYU */
#ifdef CTS4MAYU
  for (int i = 0; i < s_devNum; i++) {
    StopFeed(s_hTP[i]);
  }
#endif /* CTS4MAYU */
}


BOOL WINAPI DllMain(HANDLE module, DWORD reason, LPVOID reserve)
{
    s_instance = (HINSTANCE)module;

    return TRUE;
}
