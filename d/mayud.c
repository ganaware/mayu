///////////////////////////////////////////////////////////////////////////////
// Driver for Madotsukai no Yu^utsu for Windows2000


#include <ntddk.h>
#include <ntddkbd.h>
#include <devioctl.h>

#pragma warning(3 : 4061 4100 4132 4701 4706)

#include "keyque.c"

///////////////////////////////////////////////////////////////////////////////
// Device Extensions

struct _DetourDeviceExtension;
struct _FilterDeviceExtension;

typedef struct _DetourDeviceExtension
{
  PDRIVER_DISPATCH MajorFunction[IRP_MJ_MAXIMUM_FUNCTION + 1];

  KSPIN_LOCK lock; // lock below datum
  PDEVICE_OBJECT filterDevObj;
  LONG isOpen;
  BOOLEAN wasCleanupInitiated; //
  PIRP irpq;
  KeyQue readQue; // when IRP_MJ_READ, the contents of readQue are returned
} DetourDeviceExtension;

typedef struct _FilterDeviceExtension
{
  PDRIVER_DISPATCH MajorFunction[IRP_MJ_MAXIMUM_FUNCTION + 1];
  
  PDEVICE_OBJECT detourDevObj;
  PDEVICE_OBJECT kbdClassDevObj; // keyboard class device object
  
  KSPIN_LOCK lock; // lock below datum
  PIRP irpq;
  KeyQue readQue; // when IRP_MJ_READ, the contents of readQue are returned
  KDPC dpc;
  KTIMER timer;
  LARGE_INTEGER tickCount;
  ULONG isTouched;
} FilterDeviceExtension;

///////////////////////////////////////////////////////////////////////////////
// Protorypes (TODO)


NTSTATUS DriverEntry       (IN PDRIVER_OBJECT, IN PUNICODE_STRING);
NTSTATUS mayuAddDevice     (IN PDRIVER_OBJECT, IN PDEVICE_OBJECT);
VOID mayuUnloadDriver      (IN PDRIVER_OBJECT);
VOID mayuDetourReadCancel (IN PDEVICE_OBJECT, IN PIRP);

NTSTATUS filterGenericCompletion (IN PDEVICE_OBJECT, IN PIRP, IN PVOID);
NTSTATUS filterReadCompletion    (IN PDEVICE_OBJECT, IN PIRP, IN PVOID);

NTSTATUS mayuGenericDispatch (IN PDEVICE_OBJECT, IN PIRP);
NTSTATUS detourCreate        (IN PDEVICE_OBJECT, IN PIRP);
NTSTATUS detourClose         (IN PDEVICE_OBJECT, IN PIRP);
NTSTATUS detourRead          (IN PDEVICE_OBJECT, IN PIRP);
NTSTATUS detourWrite         (IN PDEVICE_OBJECT, IN PIRP);
NTSTATUS detourCleanup       (IN PDEVICE_OBJECT, IN PIRP);
NTSTATUS detourDeviceControl (IN PDEVICE_OBJECT, IN PIRP);
NTSTATUS filterRead          (IN PDEVICE_OBJECT, IN PIRP);
NTSTATUS filterPassThrough   (IN PDEVICE_OBJECT, IN PIRP);
NTSTATUS detourPnP           (IN PDEVICE_OBJECT, IN PIRP);
NTSTATUS filterPnP           (IN PDEVICE_OBJECT, IN PIRP);
#ifndef MAYUD_NT4
NTSTATUS detourPower         (IN PDEVICE_OBJECT, IN PIRP);
NTSTATUS filterPower         (IN PDEVICE_OBJECT, IN PIRP);
#endif // !MAYUD_NT4

VOID CancelKeyboardClassRead(IN PIRP, IN PDEVICE_OBJECT);
NTSTATUS readq(KeyQue*, PIRP);

NTSTATUS filterTouchpadCompletion (IN PDEVICE_OBJECT, IN PIRP, IN PVOID);
VOID pollTouchpad(IN PKDPC, IN PVOID, IN PVOID, IN PVOID);
NTSTATUS filterTouchpad      (IN PDEVICE_OBJECT, IN PIRP);

#ifdef ALLOC_PRAGMA
#pragma alloc_text( init, DriverEntry )
#endif // ALLOC_PRAGMA


///////////////////////////////////////////////////////////////////////////////
// Global Constants / Variables

BOOLEAN g_isPnP;
BOOLEAN g_isXp;
ULONG g_SpinLock_offset;
ULONG g_RequestIsPending_offset;

// Device names
#define UnicodeString(str) { sizeof(str) - sizeof(UNICODE_NULL),	\
			     sizeof(str) - sizeof(UNICODE_NULL), str }

static UNICODE_STRING MayuDetourDeviceName =
UnicodeString(L"\\Device\\MayuDetour0");

static UNICODE_STRING MayuDetourWin32DeviceName =
UnicodeString(L"\\DosDevices\\MayuDetour1");

static UNICODE_STRING KeyboardClassDeviceName =
UnicodeString(DD_KEYBOARD_DEVICE_NAME_U L"0");

static UNICODE_STRING UsbHubDriverName =
UnicodeString(L"\\Driver\\usbhub");

#define TICKCOUNT_FOR_TOUCHPAD_TIMEOUT 2
#define TOUCHPAD_SCANCODE 0x7d

// Global Variables
PDRIVER_DISPATCH _IopInvalidDeviceRequest; // Default dispatch function

// Ioctl value
#define IOCTL_MAYU_DETOUR_CANCEL					 \
CTL_CODE(FILE_DEVICE_KEYBOARD, 0x0001, METHOD_BUFFERED, FILE_ANY_ACCESS)


///////////////////////////////////////////////////////////////////////////////
// Entry / Unload


// initialize driver
NTSTATUS DriverEntry(IN PDRIVER_OBJECT driverObject,
		     IN PUNICODE_STRING registryPath)
{
  NTSTATUS status;
  BOOLEAN is_symbolicLinkCreated = FALSE;
  ULONG i;
  PDEVICE_OBJECT detourDevObj = NULL;
  DetourDeviceExtension *detourDevExt = NULL;
  ULONG start = 0;
  RTL_QUERY_REGISTRY_TABLE query[2];
  
  UNREFERENCED_PARAMETER(registryPath);


  // Environment specific initialize
  RtlZeroMemory(query, sizeof(query));
  query[0].Name = L"Start";
  query[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
  query[0].EntryContext = &start;
  RtlQueryRegistryValues(RTL_REGISTRY_SERVICES, L"mayud", query, NULL, NULL);
  if (start == 0x03) {
    g_isPnP = TRUE;
  } else {
    g_isPnP = FALSE;
  }
#ifdef MAYUD_NT4
  g_isXp = FALSE;
  g_SpinLock_offset = 48;
  g_RequestIsPending_offset = 0;
#else /* !MAYUD_NT4 */
  if (IoIsWdmVersionAvailable(1, 0x20)) { // is WindowsXP
    g_isXp = TRUE;
    g_SpinLock_offset = 108;
    g_RequestIsPending_offset = 0;
  } else if (IoIsWdmVersionAvailable(1, 0x10)) { // is Windows2000
    g_isXp =FALSE;
    g_SpinLock_offset = 116;
    g_RequestIsPending_offset = 48;
  } else { // Unknown version
    status = STATUS_UNKNOWN_REVISION;
    goto error;
  }
#endif /* MAYUD_NT4 */

  // initialize global variables
  _IopInvalidDeviceRequest = driverObject->MajorFunction[IRP_MJ_CREATE];
  
  // set major functions
  driverObject->DriverUnload = mayuUnloadDriver;
  if (g_isPnP == TRUE) {
    driverObject->DriverExtension->AddDevice = mayuAddDevice;
  }
  for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
#ifdef MAYUD_NT4
    if (i != IRP_MJ_POWER)
#endif // MAYUD_NT4
      driverObject->MajorFunction[i] = mayuGenericDispatch;
  if (g_isPnP == TRUE) {
    driverObject->MajorFunction[IRP_MJ_PNP] = mayuGenericDispatch;
  }
  
  // create a device
  {
    // create detour device
    status = IoCreateDevice(driverObject, sizeof(DetourDeviceExtension),
			    &MayuDetourDeviceName, FILE_DEVICE_KEYBOARD,
			    0, FALSE, &detourDevObj);
    
    if (!NT_SUCCESS(status)) goto error;
    detourDevObj->Flags |= DO_BUFFERED_IO;
#ifndef MAYUD_NT4
    detourDevObj->Flags |= DO_POWER_PAGABLE;
#endif // !MAYUD_NT4

    // initialize detour device extension
    detourDevExt = (DetourDeviceExtension*)detourDevObj->DeviceExtension;
    RtlZeroMemory(detourDevExt, sizeof(DetourDeviceExtension));
    detourDevExt->filterDevObj = NULL;

    KeInitializeSpinLock(&detourDevExt->lock);
    detourDevExt->isOpen = FALSE;
    detourDevExt->wasCleanupInitiated = FALSE;
    detourDevExt->irpq = NULL;
    status = KqInitialize(&detourDevExt->readQue);
    if (!NT_SUCCESS(status)) goto error;
    
    // create symbolic link for detour
    status =
      IoCreateSymbolicLink(&MayuDetourWin32DeviceName, &MayuDetourDeviceName);
    if (!NT_SUCCESS(status)) goto error;
    is_symbolicLinkCreated = TRUE;
    
    if (g_isPnP == FALSE)
    // attach filter device to keyboard class device
    {
      PFILE_OBJECT f;
      PDEVICE_OBJECT kbdClassDevObj;
      
      status = IoGetDeviceObjectPointer(&KeyboardClassDeviceName,
					FILE_ALL_ACCESS, &f,
					&kbdClassDevObj);
      if (!NT_SUCCESS(status)) goto error;
      ObDereferenceObject(f);
      status = mayuAddDevice(driverObject, kbdClassDevObj);
      
      // why cannot I do below ?
//      status = IoAttachDevice(filterDevObj, &KeyboardClassDeviceName,
//			      &filterDevExt->kbdClassDevObj);
      if (!NT_SUCCESS(status)) goto error;
    }
    
    // initialize Major Functions
    for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
    {
      detourDevExt->MajorFunction[i] = _IopInvalidDeviceRequest;
    }

    detourDevExt->MajorFunction[IRP_MJ_READ] = detourRead;
    detourDevExt->MajorFunction[IRP_MJ_WRITE] = detourWrite;
    detourDevExt->MajorFunction[IRP_MJ_CREATE] = detourCreate;
    detourDevExt->MajorFunction[IRP_MJ_CLOSE] = detourClose;
    detourDevExt->MajorFunction[IRP_MJ_CLEANUP] = detourCleanup;
    detourDevExt->MajorFunction[IRP_MJ_DEVICE_CONTROL] = detourDeviceControl;

#ifndef MAYUD_NT4
    detourDevExt->MajorFunction[IRP_MJ_POWER] = detourPower;
#endif // !MAYUD_NT4
    if (g_isPnP == TRUE) {
      detourDevExt->MajorFunction[IRP_MJ_PNP] = detourPnP;
    }
  }
  detourDevObj->Flags &= ~DO_DEVICE_INITIALIZING;  
  
  return STATUS_SUCCESS;
  
  error:
  {
    if (is_symbolicLinkCreated)
      IoDeleteSymbolicLink(&MayuDetourWin32DeviceName);
    if (detourDevObj)
    {
      KqFinalize(&detourDevExt->readQue);
      IoDeleteDevice(detourDevObj);
    }
  }
  return status;
}

NTSTATUS mayuAddDevice(IN PDRIVER_OBJECT driverObject,
		       IN PDEVICE_OBJECT kbdClassDevObj)
{
  NTSTATUS status;
  PDEVICE_OBJECT devObj;
  PDEVICE_OBJECT filterDevObj;
  DetourDeviceExtension *detourDevExt;
  FilterDeviceExtension *filterDevExt;
  ULONG i;

  // create filter device
  status = IoCreateDevice(driverObject, sizeof(FilterDeviceExtension),
			  NULL, FILE_DEVICE_KEYBOARD,
			  0, FALSE, &filterDevObj);
  if (!NT_SUCCESS(status)) return status;
  filterDevObj->Flags |= DO_BUFFERED_IO;
#ifndef MAYUD_NT4
  filterDevObj->Flags |= DO_POWER_PAGABLE;
#endif // !MAYUD_NT4

  // initialize filter device extension
  filterDevExt = (FilterDeviceExtension*)filterDevObj->DeviceExtension;
  RtlZeroMemory(filterDevExt, sizeof(FilterDeviceExtension));

  KeInitializeSpinLock(&filterDevExt->lock);
  filterDevExt->irpq = NULL;
  status = KqInitialize(&filterDevExt->readQue);
  if (!NT_SUCCESS(status)) goto error;
  KeInitializeDpc(&filterDevExt->dpc, pollTouchpad, (PVOID)filterDevObj);
  KeInitializeTimer(&filterDevExt->timer);
  filterDevExt->tickCount = RtlConvertLongToLargeInteger(0);
  filterDevExt->isTouched = 0;

  devObj = filterDevObj->NextDevice;
  while (devObj->NextDevice) {
    devObj = devObj->NextDevice;
  }
  filterDevExt->detourDevObj = devObj;
  detourDevExt = (DetourDeviceExtension*)devObj->DeviceExtension;
  if (!detourDevExt->filterDevObj) {
    detourDevExt->filterDevObj = filterDevObj;
  }

  filterDevExt->kbdClassDevObj =
    IoAttachDeviceToDeviceStack(filterDevObj, kbdClassDevObj);
  if (!filterDevExt->kbdClassDevObj) goto error;

  for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++) {
    filterDevExt->MajorFunction[i] =
      (filterDevExt->kbdClassDevObj->DriverObject->MajorFunction[i]
       == _IopInvalidDeviceRequest) ?
      _IopInvalidDeviceRequest : filterPassThrough;
  }
  if (RtlCompareUnicodeString(&UsbHubDriverName, &kbdClassDevObj->DriverObject->DriverName, TRUE) == 0)
  {
    filterDevExt->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = filterTouchpad;
  }
  else
  {
    filterDevExt->MajorFunction[IRP_MJ_READ] = filterRead;
  }
#ifndef MAYUD_NT4
  filterDevExt->MajorFunction[IRP_MJ_POWER] = filterPower;
#endif // !MAYUD_NT4
  if (g_isPnP == TRUE) {
    filterDevExt->MajorFunction[IRP_MJ_PNP] = filterPnP;
  }
  filterDevObj->Flags &= ~DO_DEVICE_INITIALIZING;  

  return STATUS_SUCCESS;

 error:
  if (filterDevObj) {
      KqFinalize(&filterDevExt->readQue);
      IoDeleteDevice(filterDevObj);
  }
  return status;
}

VOID CancelKeyboardClassRead(PIRP cancelIrp, PDEVICE_OBJECT kbdClassDevObj)
{
  PVOID kbdClassDevExt;
  BOOLEAN isSafe;
  PKSPIN_LOCK SpinLock;
  KIRQL currentIrql;

  kbdClassDevExt = kbdClassDevObj->DeviceExtension;
  SpinLock = (PKSPIN_LOCK)((ULONG)kbdClassDevExt + g_SpinLock_offset);
  KeAcquireSpinLock(SpinLock, &currentIrql);
  if (g_isXp == TRUE) {
    isSafe = cancelIrp->CancelRoutine ? TRUE : FALSE;
  } else {
    isSafe = *(BOOLEAN*)((ULONG)kbdClassDevExt + g_RequestIsPending_offset);
  }
  if (isSafe == TRUE) {
    KeReleaseSpinLock(SpinLock, currentIrql);
    IoCancelIrp(cancelIrp);
  } else {
    KeReleaseSpinLock(SpinLock, currentIrql);
  }
  return;
}

// unload driver
VOID mayuUnloadDriver(IN PDRIVER_OBJECT driverObject)
{
  KIRQL currentIrql;
  PIRP cancelIrp;
  PDEVICE_OBJECT devObj;
  DetourDeviceExtension *detourDevExt;

  // walk on device chain(the last one is detour device?)
  devObj = driverObject->DeviceObject;
  while (devObj->NextDevice) {
    FilterDeviceExtension *filterDevExt
      = (FilterDeviceExtension*)devObj->DeviceExtension;
    PDEVICE_OBJECT delObj;
    PDEVICE_OBJECT kbdClassDevObj;

    // detach
    IoDetachDevice(filterDevExt->kbdClassDevObj);
    // cancel filter IRP_MJ_READ
    KeAcquireSpinLock(&filterDevExt->lock, &currentIrql);
    // TODO: at this point, the irp may be completed (but what can I do for it ?)
    // finalize read que
    KqFinalize(&filterDevExt->readQue);
    cancelIrp = filterDevExt->irpq;
    filterDevExt->irpq = NULL;
    kbdClassDevObj = filterDevExt->kbdClassDevObj;
    KeReleaseSpinLock(&filterDevExt->lock, currentIrql);
    if (cancelIrp) {
      CancelKeyboardClassRead(cancelIrp, kbdClassDevObj);
    }
    // delete device objects
    delObj= devObj;
    devObj = devObj->NextDevice;
    IoDeleteDevice(delObj);
  }

  detourDevExt = (DetourDeviceExtension*)devObj->DeviceExtension;
  // cancel filter IRP_MJ_READ
  KeAcquireSpinLock(&detourDevExt->lock, &currentIrql);
  // TODO: at this point, the irp may be completed (but what can I do for it ?)
  cancelIrp = detourDevExt->irpq;
  // finalize read que
  KqFinalize(&detourDevExt->readQue);
  KeReleaseSpinLock(&detourDevExt->lock, currentIrql);
  if (cancelIrp)
    IoCancelIrp(cancelIrp);
  // delete device objects
  IoDeleteDevice(devObj);

  // delete symbolic link
  IoDeleteSymbolicLink(&MayuDetourWin32DeviceName);
}


///////////////////////////////////////////////////////////////////////////////
// Cancel Functionss


// detour read cancel
VOID mayuDetourReadCancel(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
{
  DetourDeviceExtension *devExt =
    (DetourDeviceExtension *)deviceObject->DeviceExtension;
  KIRQL currentIrql;

  IoReleaseCancelSpinLock(irp->CancelIrql);
#if 0
  KeAcquireSpinLock(&devExt->lock, &currentIrql);
  if (devExt->irpq && irp == deviceObject->CurrentIrp)
    // the current request is being cancelled
  {
    deviceObject->CurrentIrp = NULL;
    devExt->irpq = NULL;
    KeReleaseSpinLock(&devExt->lock, currentIrql);
    IoStartNextPacket(deviceObject, TRUE);
  }
  else
  {
    // Cancel a request in the device queue
    KIRQL cancelIrql;
    
    IoAcquireCancelSpinLock(&cancelIrql);
    KeRemoveEntryDeviceQueue(&deviceObject->DeviceQueue,
			     &irp->Tail.Overlay.DeviceQueueEntry);
    IoReleaseCancelSpinLock(cancelIrql);
    KeReleaseSpinLock(&devExt->lock, currentIrql);
  }
#endif
  
  irp->IoStatus.Status = STATUS_CANCELLED;
  irp->IoStatus.Information = 0;
  IoCompleteRequest(irp, IO_KEYBOARD_INCREMENT);
}

///////////////////////////////////////////////////////////////////////////////
// Complete Functions


// 
NTSTATUS filterGenericCompletion(IN PDEVICE_OBJECT deviceObject,
				 IN PIRP irp, IN PVOID context)
{
  UNREFERENCED_PARAMETER(deviceObject);
  UNREFERENCED_PARAMETER(context);
  
  if (irp->PendingReturned)
    IoMarkIrpPending(irp);
  return STATUS_SUCCESS;
}


// 
NTSTATUS filterReadCompletion(IN PDEVICE_OBJECT deviceObject,
			      IN PIRP irp, IN PVOID context)
{
  NTSTATUS status;
  KIRQL currentIrql, cancelIrql;
  PIRP irpCancel = NULL;
  FilterDeviceExtension *filterDevExt =
    (FilterDeviceExtension*)deviceObject->DeviceExtension;
  PDEVICE_OBJECT detourDevObj = filterDevExt->detourDevObj;
  DetourDeviceExtension *detourDevExt =
    (DetourDeviceExtension*)detourDevObj->DeviceExtension;

  UNREFERENCED_PARAMETER(context);
  
  KeAcquireSpinLock(&filterDevExt->lock, &currentIrql);
  filterDevExt->irpq = NULL;
  KeReleaseSpinLock(&filterDevExt->lock, currentIrql);
  if (irp->PendingReturned) {
    status = STATUS_PENDING;
    IoMarkIrpPending(irp);
  } else {
    status = STATUS_SUCCESS;
  }

  if (detourDevExt->isOpen)
  {
    KeAcquireSpinLock(&detourDevExt->lock, &currentIrql);
    // if detour is opened, key datum are forwarded to detour
    if (irp->IoStatus.Status == STATUS_SUCCESS)
    {
      PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(irp);
      
      KqEnque(&detourDevExt->readQue,
	      (KEYBOARD_INPUT_DATA *)irp->AssociatedIrp.SystemBuffer,
	      irp->IoStatus.Information / sizeof(KEYBOARD_INPUT_DATA));

      irp->IoStatus.Status = STATUS_CANCELLED;
      irp->IoStatus.Information = 0;
      detourDevExt->filterDevObj = deviceObject;
    }

    if (detourDevExt->irpq) {
      if (readq(&detourDevExt->readQue, detourDevExt->irpq) ==
	  STATUS_SUCCESS) {
	IoAcquireCancelSpinLock(&cancelIrql);
	IoSetCancelRoutine(detourDevExt->irpq, NULL);
	IoReleaseCancelSpinLock(cancelIrql);
	IoCompleteRequest(detourDevExt->irpq, IO_KEYBOARD_INCREMENT);
	detourDevExt->irpq = NULL;
      }
    }
    KeReleaseSpinLock(&detourDevExt->lock, currentIrql);

    KeAcquireSpinLock(&filterDevExt->lock, &currentIrql);
    status = readq(&filterDevExt->readQue, irp);
    KeReleaseSpinLock(&filterDevExt->lock, currentIrql);
  }

  if (status == STATUS_SUCCESS)
    irp->IoStatus.Status = STATUS_SUCCESS;
  return irp->IoStatus.Status;
}

NTSTATUS readq(KeyQue *readQue, PIRP irp)
{
  if (!KqIsEmpty(readQue)) {
    PIO_STACK_LOCATION irpSp;
    ULONG len;

    irpSp = IoGetCurrentIrpStackLocation(irp);
    len = KqDeque(readQue,
		  (KEYBOARD_INPUT_DATA *)irp->AssociatedIrp.SystemBuffer,
		  irpSp->Parameters.Read.Length / sizeof(KEYBOARD_INPUT_DATA));
    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = len * sizeof(KEYBOARD_INPUT_DATA);
    irpSp->Parameters.Read.Length = irp->IoStatus.Information;
    return STATUS_SUCCESS;
  } else {
    irp->IoStatus.Status = STATUS_PENDING;
    irp->IoStatus.Information = 0;
    return STATUS_PENDING;
  }
}

///////////////////////////////////////////////////////////////////////////////
// Dispatch Functions


// Generic Dispatcher
NTSTATUS mayuGenericDispatch(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
{
  PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(irp);

  if (deviceObject->NextDevice) {
    FilterDeviceExtension *filterDevExt =
      (FilterDeviceExtension *)deviceObject->DeviceExtension;

    return filterDevExt->MajorFunction[irpSp->MajorFunction](deviceObject, irp);
  } else {
    DetourDeviceExtension *detourDevExt = 
      (DetourDeviceExtension *)deviceObject->DeviceExtension;

    return detourDevExt->MajorFunction[irpSp->MajorFunction](deviceObject, irp);
  }
}


// detour IRP_MJ_CREATE
NTSTATUS detourCreate(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
{
  DetourDeviceExtension *detourDevExt =
    (DetourDeviceExtension*)deviceObject->DeviceExtension;

  if (1 < InterlockedIncrement(&detourDevExt->isOpen))
    // mayu detour device can be opend only once at a time
  {
    InterlockedDecrement(&detourDevExt->isOpen);
    irp->IoStatus.Status = STATUS_INTERNAL_ERROR;
  }
  else
  {
    PIRP irpCancel;
    KIRQL currentIrql;
    PDEVICE_OBJECT filterDevObj;
    
    KeAcquireSpinLock(&detourDevExt->lock, &currentIrql);
    detourDevExt->wasCleanupInitiated = FALSE;
    KqClear(&detourDevExt->readQue);
    filterDevObj = detourDevExt->filterDevObj;
    KeReleaseSpinLock(&detourDevExt->lock, currentIrql);
    if (filterDevObj) {
      FilterDeviceExtension *filterDevExt =
	(FilterDeviceExtension*)filterDevObj->DeviceExtension;
      PDEVICE_OBJECT kbdClassDevObj;

      KeAcquireSpinLock(&filterDevExt->lock, &currentIrql);
      irpCancel = filterDevExt->kbdClassDevObj->CurrentIrp;
      kbdClassDevObj = filterDevExt->kbdClassDevObj;
      KeReleaseSpinLock(&filterDevExt->lock, currentIrql);
      if (irpCancel) {
	CancelKeyboardClassRead(irpCancel, kbdClassDevObj);
      }
    }

    irp->IoStatus.Status = STATUS_SUCCESS;
  }
  irp->IoStatus.Information = 0;
  IoCompleteRequest(irp, IO_NO_INCREMENT);
  return irp->IoStatus.Status;
}


// detour IRP_MJ_CLOSE
NTSTATUS detourClose(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
{
  DetourDeviceExtension *detourDevExt =
    (DetourDeviceExtension*)deviceObject->DeviceExtension;

  InterlockedDecrement(&detourDevExt->isOpen);
  irp->IoStatus.Status = STATUS_SUCCESS;
  irp->IoStatus.Information = 0;
  IoCompleteRequest(irp, IO_NO_INCREMENT);
  return STATUS_SUCCESS;
}


// detour IRP_MJ_READ
NTSTATUS detourRead(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
{
  NTSTATUS status;
  PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(irp);
  DetourDeviceExtension *detourDevExt =
    (DetourDeviceExtension*)deviceObject->DeviceExtension;
  KIRQL currentIrql, cancelIrql;

  KeAcquireSpinLock(&detourDevExt->lock, &currentIrql);
  if (irpSp->Parameters.Read.Length == 0)
    status = STATUS_SUCCESS;
  else if (irpSp->Parameters.Read.Length % sizeof(KEYBOARD_INPUT_DATA))
    status = STATUS_BUFFER_TOO_SMALL;
  else
    status = readq(&detourDevExt->readQue, irp);
  if (status == STATUS_PENDING) {
    IoAcquireCancelSpinLock(&cancelIrql);
    IoMarkIrpPending(irp);
    detourDevExt->irpq = irp;
    IoSetCancelRoutine(irp, mayuDetourReadCancel);
    IoReleaseCancelSpinLock(cancelIrql);
  }
  else {
    IoCompleteRequest(irp, IO_KEYBOARD_INCREMENT);
  }
  KeReleaseSpinLock(&detourDevExt->lock, currentIrql);
  return status;
}


// detour IRP_MJ_WRITE
NTSTATUS detourWrite(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
{
  NTSTATUS status;
  PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(irp);
  ULONG len = irpSp->Parameters.Write.Length;
  DetourDeviceExtension *detourDevExt =
    (DetourDeviceExtension*)deviceObject->DeviceExtension;
  
  irp->IoStatus.Information = 0;
  if (len == 0)
    status = STATUS_SUCCESS;
  else if (len % sizeof(KEYBOARD_INPUT_DATA))
    status = STATUS_INVALID_PARAMETER;
  else {
    // write to filter que
    KIRQL cancelIrql, currentIrql;
    PIRP irpCancel;
    PDEVICE_OBJECT filterDevObj;

    KeAcquireSpinLock(&detourDevExt->lock, &currentIrql);
    filterDevObj = detourDevExt->filterDevObj;
    KeReleaseSpinLock(&detourDevExt->lock, currentIrql);
    // enque filter que
    if (filterDevObj) {
      FilterDeviceExtension *filterDevExt =
	(FilterDeviceExtension*)filterDevObj->DeviceExtension;
      PDEVICE_OBJECT kbdClassDevObj;

      KeAcquireSpinLock(&filterDevExt->lock, &currentIrql);

      len /= sizeof(KEYBOARD_INPUT_DATA);
      len = KqEnque(&filterDevExt->readQue,
		    (KEYBOARD_INPUT_DATA *)irp->AssociatedIrp.SystemBuffer,
		    len);
      irp->IoStatus.Information = len * sizeof(KEYBOARD_INPUT_DATA);
      irpSp->Parameters.Write.Length = irp->IoStatus.Information;
      // cancel filter irp
      irpCancel = filterDevExt->irpq; 
      filterDevExt->irpq = NULL;
      kbdClassDevObj = filterDevExt->kbdClassDevObj;
      KeReleaseSpinLock(&filterDevExt->lock, currentIrql);
      if (irpCancel) {
	CancelKeyboardClassRead(irpCancel, kbdClassDevObj);
      }
      status = STATUS_SUCCESS;
    } else {
      irp->IoStatus.Information = 0;
      irpSp->Parameters.Write.Length = irp->IoStatus.Information;
      status = STATUS_CANCELLED;
    }
  }
  IoCompleteRequest(irp, IO_NO_INCREMENT);
  return status;
}


// detour IRP_MJ_CLEANUP
NTSTATUS detourCleanup(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
{
  KIRQL currentIrql, cancelIrql;
  PIO_STACK_LOCATION irpSp;
  PIRP  currentIrp = NULL, irpCancel;
  DetourDeviceExtension *detourDevExt =
    (DetourDeviceExtension*)deviceObject->DeviceExtension;

  KeAcquireSpinLock(&detourDevExt->lock, &currentIrql);
  IoAcquireCancelSpinLock(&cancelIrql);
  irpSp = IoGetCurrentIrpStackLocation(irp);
  detourDevExt->wasCleanupInitiated = TRUE;
  
  // Complete all requests queued by this thread with STATUS_CANCELLED
  currentIrp = deviceObject->CurrentIrp;
  deviceObject->CurrentIrp = NULL;
  detourDevExt->irpq = NULL;
  
  while (currentIrp != NULL)
  {
    IoSetCancelRoutine(currentIrp, NULL);
    currentIrp->IoStatus.Status = STATUS_CANCELLED;
    currentIrp->IoStatus.Information = 0;
    
    IoReleaseCancelSpinLock(cancelIrql);
    KeReleaseSpinLock(&detourDevExt->lock, currentIrql);
    IoCompleteRequest(currentIrp, IO_NO_INCREMENT);
    KeAcquireSpinLock(&detourDevExt->lock, &currentIrql);
    IoAcquireCancelSpinLock(&cancelIrql);
    
    // Dequeue the next packet (IRP) from the device work queue.
    {
      PKDEVICE_QUEUE_ENTRY packet =
	KeRemoveDeviceQueue(&deviceObject->DeviceQueue);
      currentIrp = packet ?
	CONTAINING_RECORD(packet, IRP, Tail.Overlay.DeviceQueueEntry) : NULL;
    }
  }
  
  IoReleaseCancelSpinLock(cancelIrql);
  KeReleaseSpinLock(&detourDevExt->lock, currentIrql);
  
  // Complete the cleanup request with STATUS_SUCCESS.
  irp->IoStatus.Status = STATUS_SUCCESS;
  irp->IoStatus.Information = 0;
  IoCompleteRequest(irp, IO_NO_INCREMENT);
  
  return STATUS_SUCCESS;
}


// detour IRP_MJ_DEVICE_CONTROL
NTSTATUS detourDeviceControl(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
{
  NTSTATUS status;
  PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(irp);
  DetourDeviceExtension *detourDevExt =
    (DetourDeviceExtension*)deviceObject->DeviceExtension;
  
  irp->IoStatus.Information = 0;
  switch (irpSp->Parameters.DeviceIoControl.IoControlCode)
  {
    case IOCTL_MAYU_DETOUR_CANCEL:
    {
      KIRQL currentIrql;
      PIRP irpCancel = NULL;
      
      KeAcquireSpinLock(&detourDevExt->lock, &currentIrql);
      if (detourDevExt->isOpen)
	irpCancel = detourDevExt->irpq;
      KeReleaseSpinLock(&detourDevExt->lock, currentIrql);
      
      if (irpCancel)
	IoCancelIrp(irpCancel);// at this point, the irpCancel may be completed
      status = STATUS_SUCCESS;
      break;
    }
    default:
      status = STATUS_INVALID_DEVICE_REQUEST;
      break;
  }
  irp->IoStatus.Status = status;
  IoCompleteRequest(irp, IO_NO_INCREMENT);
  
  return STATUS_SUCCESS;
}


#ifndef MAYUD_NT4
// detour IRP_MJ_POWER
NTSTATUS detourPower(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
{
  UNREFERENCED_PARAMETER(deviceObject);
  
  PoStartNextPowerIrp(irp);
  irp->IoStatus.Status = STATUS_SUCCESS;
  irp->IoStatus.Information = 0;
  IoCompleteRequest(irp, IO_NO_INCREMENT);
  return STATUS_SUCCESS;
}
#endif // !MAYUD_NT4


// filter IRP_MJ_READ
NTSTATUS filterRead(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
{
  NTSTATUS status;
  PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(irp);
  FilterDeviceExtension *filterDevExt =
    (FilterDeviceExtension*)deviceObject->DeviceExtension;
  DetourDeviceExtension *detourDevExt =
    (DetourDeviceExtension*)filterDevExt->detourDevObj->DeviceExtension;
  KIRQL currentIrql;
  
  if (detourDevExt->isOpen && !detourDevExt->wasCleanupInitiated)
    // read from que
  {
    ULONG len = irpSp->Parameters.Read.Length;
    
    irp->IoStatus.Information = 0;
    KeAcquireSpinLock(&filterDevExt->lock, &currentIrql);
    if (len == 0)
      status = STATUS_SUCCESS;
    else if (len % sizeof(KEYBOARD_INPUT_DATA))
      status = STATUS_BUFFER_TOO_SMALL;
    else
      status = readq(&filterDevExt->readQue, irp);
    KeReleaseSpinLock(&filterDevExt->lock, currentIrql);
    if (status != STATUS_PENDING) {
      irp->IoStatus.Status = status;
      IoCompleteRequest(irp, IO_NO_INCREMENT);
      return status;
    }
  }
  KeAcquireSpinLock(&filterDevExt->lock, &currentIrql);
  filterDevExt->irpq = irp;
  KeReleaseSpinLock(&filterDevExt->lock, currentIrql);
  
  *IoGetNextIrpStackLocation(irp) = *irpSp;
  IoSetCompletionRoutine(irp, filterReadCompletion, NULL, TRUE, TRUE, TRUE);
  return IoCallDriver(filterDevExt->kbdClassDevObj, irp);
}


// pass throught irp to keyboard class driver
NTSTATUS filterPassThrough(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
{
  FilterDeviceExtension *filterDevExt =
    (FilterDeviceExtension*)deviceObject->DeviceExtension;

  *IoGetNextIrpStackLocation(irp) = *IoGetCurrentIrpStackLocation(irp);
  IoSetCompletionRoutine(irp, filterGenericCompletion,
			 NULL, TRUE, TRUE, TRUE);
  return IoCallDriver(filterDevExt->kbdClassDevObj, irp);
}


#ifndef MAYUD_NT4
// filter IRP_MJ_POWER
NTSTATUS filterPower(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
{
  FilterDeviceExtension *filterDevExt =
    (FilterDeviceExtension*)deviceObject->DeviceExtension;

  PoStartNextPowerIrp(irp);
  IoSkipCurrentIrpStackLocation(irp);
  return PoCallDriver(filterDevExt->kbdClassDevObj, irp);
}
#endif // !MAYUD_NT4
NTSTATUS filterPnP(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
{
  PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(irp);
  FilterDeviceExtension *filterDevExt =
    (FilterDeviceExtension*)deviceObject->DeviceExtension;
  DetourDeviceExtension *detourDevExt =
    (DetourDeviceExtension*)filterDevExt->detourDevObj->DeviceExtension;
  KIRQL currentIrql;
  NTSTATUS status;
  ULONG minor;
  PIRP cancelIrp;

  minor = irpSp->MinorFunction;
  IoSkipCurrentIrpStackLocation(irp);
  status = IoCallDriver(filterDevExt->kbdClassDevObj, irp);
  switch (minor) {
  case IRP_MN_SURPRISE_REMOVAL:
  case IRP_MN_REMOVE_DEVICE:
    KeAcquireSpinLock(&detourDevExt->lock, &currentIrql);
    if (detourDevExt->filterDevObj == deviceObject) {
      PDEVICE_OBJECT devObj = deviceObject->DriverObject->DeviceObject;

      detourDevExt->filterDevObj = NULL;
      while (devObj->NextDevice) {
	if (devObj != deviceObject) {
	  detourDevExt->filterDevObj = devObj;
	  break;
	}
	devObj = devObj->NextDevice;
      }
    }
    KeReleaseSpinLock(&detourDevExt->lock, currentIrql);
    // detach
    IoDetachDevice(filterDevExt->kbdClassDevObj);

    KeAcquireSpinLock(&filterDevExt->lock, &currentIrql);
    // TODO: at this point, the irp may be completed (but what can I do for it ?)
    cancelIrp = filterDevExt->irpq;
    KqFinalize(&filterDevExt->readQue);

    KeReleaseSpinLock(&filterDevExt->lock, currentIrql);
    if (cancelIrp) {
      IoCancelIrp(cancelIrp);
    }
    IoDeleteDevice(deviceObject);
    break;
  default:
    break;
  }
  return status;
}


// 
NTSTATUS filterTouchpadCompletion(IN PDEVICE_OBJECT deviceObject,
				 IN PIRP irp, IN PVOID context)
{
  FilterDeviceExtension *filterDevExt =
    (FilterDeviceExtension*)deviceObject->DeviceExtension;

  UNREFERENCED_PARAMETER(context);
  
  if (irp->PendingReturned)
    IoMarkIrpPending(irp);

  KeQueryTickCount(&filterDevExt->tickCount);
  if (filterDevExt->isTouched == 0)
  {
    KIRQL currentIrql, cancelIrql;
    PDEVICE_OBJECT detourDevObj = filterDevExt->detourDevObj;
    DetourDeviceExtension *detourDevExt =
      (DetourDeviceExtension*)detourDevObj->DeviceExtension;
    if (detourDevExt->isOpen)
    {
      KEYBOARD_INPUT_DATA PadKey = {0, TOUCHPAD_SCANCODE, 0, 0, 0};
      KeAcquireSpinLock(&detourDevExt->lock, &currentIrql);
      // if detour is opened, key datum are forwarded to detour
      KqEnque(&detourDevExt->readQue, &PadKey, 1);
      detourDevExt->filterDevObj = deviceObject;

      if (detourDevExt->irpq) {
	if (readq(&detourDevExt->readQue, detourDevExt->irpq) ==
	    STATUS_SUCCESS) {
	  IoAcquireCancelSpinLock(&cancelIrql);
	  IoSetCancelRoutine(detourDevExt->irpq, NULL);
	  IoReleaseCancelSpinLock(cancelIrql);
	  IoCompleteRequest(detourDevExt->irpq, IO_KEYBOARD_INCREMENT);
	  detourDevExt->irpq = NULL;
	}
      }
      KeReleaseSpinLock(&detourDevExt->lock, currentIrql);
    }
    filterDevExt->isTouched = 1;
    KeSetTimer(&filterDevExt->timer, RtlConvertLongToLargeInteger(-100), &filterDevExt->dpc);
  }
  return STATUS_SUCCESS;
}


// filter touchpad input
NTSTATUS filterTouchpad(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
{
  FilterDeviceExtension *filterDevExt =
    (FilterDeviceExtension*)deviceObject->DeviceExtension;

  *IoGetNextIrpStackLocation(irp) = *IoGetCurrentIrpStackLocation(irp);
  IoSetCompletionRoutine(irp, filterTouchpadCompletion,
			 NULL, TRUE, TRUE, TRUE);
  return IoCallDriver(filterDevExt->kbdClassDevObj, irp);
}


VOID pollTouchpad(IN PKDPC Dpc,
		  IN PVOID DeferredContext,
		  IN PVOID SystemArgument1,
		  IN PVOID SystemArgument2)
{
  LARGE_INTEGER tickCount = RtlConvertLongToLargeInteger(0);
  PDEVICE_OBJECT filterDevObj = (PDEVICE_OBJECT)DeferredContext;
  FilterDeviceExtension *filterDevExt =
    (FilterDeviceExtension*)filterDevObj->DeviceExtension;
  UNREFERENCED_PARAMETER(Dpc);
  UNREFERENCED_PARAMETER(SystemArgument1);
  UNREFERENCED_PARAMETER(SystemArgument2);

  KeQueryTickCount(&tickCount);
  if (tickCount.QuadPart - filterDevExt->tickCount.QuadPart
      > TICKCOUNT_FOR_TOUCHPAD_TIMEOUT)
  {
    KIRQL currentIrql, cancelIrql;
    PDEVICE_OBJECT detourDevObj = filterDevExt->detourDevObj;
    DetourDeviceExtension *detourDevExt =
      (DetourDeviceExtension*)detourDevObj->DeviceExtension;
    if (detourDevExt->isOpen)
    {
      KEYBOARD_INPUT_DATA PadKey = {0, TOUCHPAD_SCANCODE, 1, 0, 0};
      KeAcquireSpinLock(&detourDevExt->lock, &currentIrql);
      // if detour is opened, key datum are forwarded to detour
      KqEnque(&detourDevExt->readQue, &PadKey, 1);
      detourDevExt->filterDevObj = filterDevObj;

      if (detourDevExt->irpq) {
	if (readq(&detourDevExt->readQue, detourDevExt->irpq) ==
	    STATUS_SUCCESS) {
	  IoAcquireCancelSpinLock(&cancelIrql);
	  IoSetCancelRoutine(detourDevExt->irpq, NULL);
	  IoReleaseCancelSpinLock(cancelIrql);
	  IoCompleteRequest(detourDevExt->irpq, IO_KEYBOARD_INCREMENT);
	  detourDevExt->irpq = NULL;
	}
      }
      KeReleaseSpinLock(&detourDevExt->lock, currentIrql);
    }
    filterDevExt->isTouched = 0;
  }
  else
  {
    KeSetTimer(&filterDevExt->timer, RtlConvertLongToLargeInteger(-1000), &filterDevExt->dpc);
  }
}

NTSTATUS detourPnP(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
{
  UNREFERENCED_PARAMETER(deviceObject);

  IoSkipCurrentIrpStackLocation(irp);
  irp->IoStatus.Status = STATUS_SUCCESS;
  irp->IoStatus.Information = 0;
  IoCompleteRequest(irp, IO_NO_INCREMENT);
  return STATUS_SUCCESS;
}
