///////////////////////////////////////////////////////////////////////////////
// Driver for Madotsukai no Yu^utsu for Windows2000


#include <ntddk.h>
#include <ntddkbd.h>
#include <devioctl.h>

#pragma warning(3 : 4061 4100 4132 4701 4706)

#include "keyque.c"


///////////////////////////////////////////////////////////////////////////////
// Protorypes (TODO)


NTSTATUS DriverEntry       (IN PDRIVER_OBJECT, IN PUNICODE_STRING);
VOID mayuUnloadDriver      (IN PDRIVER_OBJECT);
VOID mayuGenericReadCancel (IN PDEVICE_OBJECT, IN PIRP);
VOID mayuGenericStartIo    (IN PDEVICE_OBJECT, IN PIRP);

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
#ifndef MAYUD_NT4
NTSTATUS detourPower         (IN PDEVICE_OBJECT, IN PIRP);
NTSTATUS filterPower         (IN PDEVICE_OBJECT, IN PIRP);
NTSTATUS detourPnP           (IN PDEVICE_OBJECT, IN PIRP);
NTSTATUS filterPnP           (IN PDEVICE_OBJECT, IN PIRP);
NTSTATUS mayuAddDevice       (IN PDRIVER_OBJECT, IN PDEVICE_OBJECT);
#endif // !MAYUD_NT4


#ifdef ALLOC_PRAGMA
#pragma alloc_text( init, DriverEntry )
#endif // ALLOC_PRAGMA


///////////////////////////////////////////////////////////////////////////////
// Device Extensions


struct _MayuDeviceExtension;

#define MAX_KBD 4

#define KD_SET(x, y) ((x) |= (1 << (y)))
#define KD_CLR(x, y) ((x) &= ~(1 << (y)))
#define KD_ISSET(x, y) ((x) & (1 << (y)))
#define KD_ZERO(x) ((x) = 0)

typedef struct CommonData
{
  // Device Objects
  PDEVICE_OBJECT detourDevObj;
  PDEVICE_OBJECT filterDevObj[MAX_KBD];
  
  // Device Extensions
  struct _MayuDeviceExtension *detourDevExt;
  struct _MayuDeviceExtension *filterDevExt[MAX_KBD];
  ULONG kbd_desc;
  ULONG curkbd;
  
  LONG isDetourOpened; // is detour opened ?
} CommonData;

typedef struct _MayuDeviceExtension
{
  PDRIVER_DISPATCH MajorFunction[IRP_MJ_MAXIMUM_FUNCTION + 1];
  
  CommonData *cd; // common data
  PDEVICE_OBJECT kbdClassDevObj; // keyboard class device object
  ULONG kbdnum;
  
  KSPIN_LOCK lock; // lock below datum
  BOOLEAN isReadRequestPending; //
  BOOLEAN wasCleanupInitiated; //
  KeyQue readQue; // when IRP_MJ_READ, the contents of readQue are returned
} MayuDeviceExtension;

#define getCd(DeviceObject)					\
(((MayuDeviceExtension *)(DeviceObject->DeviceExtension))->cd)
#define getKbdClassDevObj(DeviceObject)					\
(((MayuDeviceExtension *)(DeviceObject->DeviceExtension))->kbdClassDevObj)


///////////////////////////////////////////////////////////////////////////////
// Global Constants / Variables


// Device names
#define UnicodeString(str) { sizeof(str) - sizeof(UNICODE_NULL),	\
			     sizeof(str) - sizeof(UNICODE_NULL), str }

static UNICODE_STRING MayuDetourDeviceName =
UnicodeString(L"\\Device\\MayuDetour0");

static UNICODE_STRING MayuDetourWin32DeviceName =
UnicodeString(L"\\DosDevices\\MayuDetour1");

static UNICODE_STRING KeyboardClassDeviceName =
UnicodeString(DD_KEYBOARD_DEVICE_NAME_U L"0");


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
  CommonData *cd = NULL;
  
  UNREFERENCED_PARAMETER(registryPath);
  
  // initialize global variables
  _IopInvalidDeviceRequest = driverObject->MajorFunction[IRP_MJ_CREATE];
  
  // set major functions
  driverObject->DriverUnload = mayuUnloadDriver;
  driverObject->DriverStartIo = mayuGenericStartIo;
  driverObject->DriverExtension->AddDevice = mayuAddDevice;
  for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
#ifdef MAYUD_NT4
    if (i != IRP_MJ_POWER)
#endif // MAYUD_NT4
      driverObject->MajorFunction[i] = mayuGenericDispatch;
#ifndef MAYUD_NT4
  driverObject->MajorFunction[IRP_MJ_PNP] = mayuGenericDispatch;
#endif // MAYUD_NT4
  
  // create a device
  {
    // initialize common data
    cd = ExAllocatePool(NonPagedPool, sizeof(CommonData));
    if (!cd) { status = STATUS_INSUFFICIENT_RESOURCES; goto error; }
    RtlZeroMemory(cd, sizeof(CommonData));
    
    // create detour device
    status = IoCreateDevice(driverObject, sizeof(MayuDeviceExtension),
			    &MayuDetourDeviceName, FILE_DEVICE_KEYBOARD,
			    0, FALSE, &cd->detourDevObj);
    if (!NT_SUCCESS(status)) goto error;
    cd->detourDevObj->Flags |= DO_BUFFERED_IO;
#ifndef MAYUD_NT4
    cd->detourDevObj->Flags |= DO_POWER_PAGABLE;
	cd->detourDevObj->Flags &= ~DO_DEVICE_INITIALIZING;
#endif // !MAYUD_NT4
    
    // initialize detour device extension
    cd->detourDevExt = (MayuDeviceExtension*)cd->detourDevObj->DeviceExtension;
    RtlZeroMemory(cd->detourDevExt, sizeof(MayuDeviceExtension));
    cd->detourDevExt->cd = cd;
    KD_ZERO(cd->kbd_desc);
    cd->curkbd = 0;
    KeInitializeSpinLock(&cd->detourDevExt->lock);
    status = KqInitialize(&cd->detourDevExt->readQue);
    if (!NT_SUCCESS(status)) goto error;
    
    // create symbolic link for detour
    status =
      IoCreateSymbolicLink(&MayuDetourWin32DeviceName, &MayuDetourDeviceName);
    if (!NT_SUCCESS(status)) goto error;
    is_symbolicLinkCreated = TRUE;
    
    // initialize Major Functions
    for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
      cd->detourDevExt->MajorFunction[i] = _IopInvalidDeviceRequest;
    
    cd->detourDevExt->MajorFunction[IRP_MJ_READ] = detourRead;
    cd->detourDevExt->MajorFunction[IRP_MJ_WRITE] = detourWrite;
    cd->detourDevExt->MajorFunction[IRP_MJ_CREATE] = detourCreate;
    cd->detourDevExt->MajorFunction[IRP_MJ_CLOSE] = detourClose;
    cd->detourDevExt->MajorFunction[IRP_MJ_CLEANUP] = detourCleanup;
    cd->detourDevExt->MajorFunction[IRP_MJ_DEVICE_CONTROL] =
      detourDeviceControl;
    
#ifndef MAYUD_NT4
    cd->detourDevExt->MajorFunction[IRP_MJ_POWER] = detourPower;
	cd->detourDevExt->MajorFunction[IRP_MJ_PNP] = detourPnP;
#endif // !MAYUD_NT4
  }

  return STATUS_SUCCESS;
  
  error:
  if (cd)
  {
    if (is_symbolicLinkCreated)
      IoDeleteSymbolicLink(&MayuDetourWin32DeviceName);
    if (cd->detourDevObj)
    {
      KqFinalize(&cd->detourDevExt->readQue);
      IoDeleteDevice(cd->detourDevObj);
    }
    ExFreePool(cd);
  }
  return status;
}

NTSTATUS mayuAddDevice(IN PDRIVER_OBJECT driverObject, IN PDEVICE_OBJECT PDO)
{
    NTSTATUS status;
	ULONG i;
	CommonData *cd = getCd(driverObject->DeviceObject);
	int free_kbd;

	for (free_kbd = 0; free_kbd < MAX_KBD; free_kbd++)
	  if (!KD_ISSET(cd->kbd_desc, free_kbd))
	    break;

	if (free_kbd == MAX_KBD) 
		return (STATUS_NO_SUCH_DEVICE);
    // create filter device
    status = IoCreateDevice(driverObject, sizeof(MayuDeviceExtension),
			    NULL, FILE_DEVICE_KEYBOARD,
			    0, FALSE, &cd->filterDevObj[free_kbd]);
    if (!NT_SUCCESS(status)) goto error;
    
    // initialize filter device extension
    cd->filterDevExt[free_kbd] = (MayuDeviceExtension*)cd->filterDevObj[free_kbd]->DeviceExtension;
    RtlZeroMemory(cd->filterDevExt[free_kbd], sizeof(MayuDeviceExtension));
    cd->filterDevExt[free_kbd]->cd = cd;
    cd->filterDevExt[free_kbd]->kbdnum = free_kbd;
    KeInitializeSpinLock(&cd->filterDevExt[free_kbd]->lock);
    status = KqInitialize(&cd->filterDevExt[free_kbd]->readQue);
    if (!NT_SUCCESS(status)) goto error;

    // attach filter device to keyboard class device
    cd->filterDevExt[free_kbd]->kbdClassDevObj = IoAttachDeviceToDeviceStack(cd->filterDevObj[free_kbd], PDO);
	cd->filterDevObj[free_kbd]->Flags |= (DO_BUFFERED_IO | DO_POWER_PAGABLE);
	cd->filterDevObj[free_kbd]->Flags &= ~DO_DEVICE_INITIALIZING;
    for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
      cd->filterDevExt[free_kbd]->MajorFunction[i] =
		(cd->filterDevExt[free_kbd]->kbdClassDevObj->DriverObject->MajorFunction[i] == _IopInvalidDeviceRequest) ?
		_IopInvalidDeviceRequest : filterPassThrough;

    cd->filterDevExt[free_kbd]->MajorFunction[IRP_MJ_READ] = filterRead;
    cd->filterDevExt[free_kbd]->MajorFunction[IRP_MJ_POWER] = filterPower;
	cd->filterDevExt[free_kbd]->MajorFunction[IRP_MJ_PNP] = filterPnP;
	KD_SET(cd->kbd_desc, free_kbd);
	return (STATUS_SUCCESS);

error:
    if (cd->filterDevObj)
    {
      KqFinalize(&cd->filterDevExt[free_kbd]->readQue);
      IoDeleteDevice(cd->filterDevObj[free_kbd]);
    }
	return status;
}

// unload driver
VOID mayuUnloadDriver(IN PDRIVER_OBJECT driverObject)
{
  PIRP irp;
  CommonData *cd = getCd(driverObject->DeviceObject);
  ULONG i;
  
  // delete symbolic link
  IoDeleteSymbolicLink(&MayuDetourWin32DeviceName);
  
  // cancel filter IRP_MJ_READ
  for (i = 0; i < MAX_KBD; ++i) {
    // detach
    if (KD_ISSET(cd->kbd_desc, i)) {
	IoDetachDevice(cd->filterDevExt[i]->kbdClassDevObj);
  
	irp = cd->filterDevExt[i]->kbdClassDevObj->CurrentIrp;
	// TODO: at this point, the irp may be completed (but what can I do for it ?)
	if (irp)
		IoCancelIrp(irp);
	KqFinalize(&cd->filterDevExt[i]->readQue);
	IoDeleteDevice(cd->filterDevObj[i]);
    }
  }
  
  // finalize read que
  KqFinalize(&cd->detourDevExt->readQue);
  
  // delete device objects
  IoDeleteDevice(cd->detourDevObj);

  ExFreePool(cd);
}


///////////////////////////////////////////////////////////////////////////////
// Cancel Functionss


// read cancel
VOID mayuGenericReadCancel(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
{
  CommonData *cd = getCd(deviceObject);
  MayuDeviceExtension *devExt =
    (MayuDeviceExtension *)deviceObject->DeviceExtension;
  KIRQL currentIrql;
  
  IoReleaseCancelSpinLock(irp->CancelIrql);
  KeAcquireSpinLock(&devExt->lock, &currentIrql);
  if (devExt->isReadRequestPending && irp == deviceObject->CurrentIrp)
    // the current request is being cancelled
  {
    deviceObject->CurrentIrp = NULL;
    devExt->isReadRequestPending = FALSE;
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
  
  irp->IoStatus.Status = STATUS_CANCELLED;
  irp->IoStatus.Information = 0;
  IoCompleteRequest(irp, IO_KEYBOARD_INCREMENT);
}


///////////////////////////////////////////////////////////////////////////////
// Start IO Functions


VOID mayuGenericStartIo(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
{
  KIRQL cancelIrql;
  CommonData *cd = getCd(deviceObject);
  MayuDeviceExtension *devExt =
    (MayuDeviceExtension *)deviceObject->DeviceExtension;
  
  KeAcquireSpinLockAtDpcLevel(&devExt->lock);
  IoAcquireCancelSpinLock(&cancelIrql);
  
  if (irp->Cancel || devExt->wasCleanupInitiated)
    // cancelled ? or cleanuped ?
  {
    IoReleaseCancelSpinLock(cancelIrql);
    KeReleaseSpinLockFromDpcLevel(&devExt->lock);
    return;
  }
  
  // empty ?
  if (KqIsEmpty(&devExt->readQue))
  {
    devExt->isReadRequestPending = TRUE;
    IoReleaseCancelSpinLock(cancelIrql);
    KeReleaseSpinLockFromDpcLevel(&devExt->lock);
    return;
  }
  
  {
    PIO_STACK_LOCATION irpSp;
    ULONG len;
    
    IoSetCancelRoutine(irp, NULL);
    deviceObject->CurrentIrp = NULL;
    IoReleaseCancelSpinLock(cancelIrql);
    
    irpSp = IoGetCurrentIrpStackLocation(irp);

    len = KqDeque(&devExt->readQue,
		  (KEYBOARD_INPUT_DATA *)irp->AssociatedIrp.SystemBuffer,
		  irpSp->Parameters.Read.Length / sizeof(KEYBOARD_INPUT_DATA));
    
    devExt->isReadRequestPending = FALSE;
    
    KeReleaseSpinLockFromDpcLevel(&devExt->lock);
    
    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = len * sizeof(KEYBOARD_INPUT_DATA);
    irpSp->Parameters.Read.Length = irp->IoStatus.Information;
    IoStartNextPacket(deviceObject, TRUE);
    IoCompleteRequest(irp, IO_KEYBOARD_INCREMENT);
  }
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
  UNREFERENCED_PARAMETER(context);
  
  if (irp->PendingReturned)
    IoMarkIrpPending(irp);
  if (irp->IoStatus.Status == STATUS_SUCCESS)
  {
    KIRQL currentIrql, cancelIrql;
    PIRP irpCancel = NULL;
    CommonData *cd = getCd(deviceObject);
    MayuDeviceExtension *devExt = cd->detourDevExt;
    
    KeAcquireSpinLock(&devExt->lock, &currentIrql);
    IoAcquireCancelSpinLock(&cancelIrql);
    if (cd->isDetourOpened)
      // if detour is opened, key datum are forwarded to detour
    {
      PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(irp);
      
      KqEnque(&devExt->readQue,
	      (KEYBOARD_INPUT_DATA *)irp->AssociatedIrp.SystemBuffer,
	      irp->IoStatus.Information / sizeof(KEYBOARD_INPUT_DATA));
      
      irp->IoStatus.Status = STATUS_CANCELLED;
      irp->IoStatus.Information = 0;
      irpCancel = cd->detourDevObj->CurrentIrp;
    }
    IoReleaseCancelSpinLock(cancelIrql);
    KeReleaseSpinLock(&devExt->lock, currentIrql);
    if (irpCancel)
      IoCancelIrp(irpCancel); // at this point, the irpCancel may be completed
  }
  return STATUS_SUCCESS;
}


///////////////////////////////////////////////////////////////////////////////
// Dispatch Functions


// Generic Dispatcher
NTSTATUS mayuGenericDispatch(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
{
  PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(irp);
  MayuDeviceExtension *devExt =
    (MayuDeviceExtension *)deviceObject->DeviceExtension;
  return devExt->MajorFunction[irpSp->MajorFunction](deviceObject, irp);
}


// detour IRP_MJ_CREATE
NTSTATUS detourCreate(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
{
  ULONG i;

  CommonData *cd = getCd(deviceObject);
  if (1 < InterlockedIncrement(&cd->isDetourOpened))
    // mayu detour device can be opend only once at a time
  {
    InterlockedDecrement(&cd->isDetourOpened);
    irp->IoStatus.Status = STATUS_INTERNAL_ERROR;
  }
  else
  {
    PIRP irpCancel;
    KIRQL currentIrql;
    
    cd->detourDevExt->wasCleanupInitiated = FALSE;
	for (i = 0; i < MAX_KBD; ++i) {
	  if (KD_ISSET(cd->kbd_desc, i)) {
		irpCancel = cd->filterDevExt[i]->kbdClassDevObj->CurrentIrp;
		if (irpCancel) // cancel filter's read irp
			IoCancelIrp(irpCancel);
	  }
	}
    
    KeAcquireSpinLock(&cd->detourDevExt->lock, &currentIrql);
    KqClear(&cd->detourDevExt->readQue);
    KeReleaseSpinLock(&cd->detourDevExt->lock, currentIrql);

    irp->IoStatus.Status = STATUS_SUCCESS;
  }
  irp->IoStatus.Information = 0;
  IoCompleteRequest(irp, IO_NO_INCREMENT);
  return irp->IoStatus.Status;
}


// detour IRP_MJ_CLOSE
NTSTATUS detourClose(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
{
  CommonData *cd = getCd(deviceObject);
  InterlockedDecrement(&cd->isDetourOpened);
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
  CommonData *cd = getCd(deviceObject);
  
  if (irpSp->Parameters.Read.Length == 0)
    status = STATUS_SUCCESS;
  else if (irpSp->Parameters.Read.Length % sizeof(KEYBOARD_INPUT_DATA))
    status = STATUS_BUFFER_TOO_SMALL;
  else
    status = STATUS_PENDING;
  irp->IoStatus.Status = status;
  irp->IoStatus.Information = 0;
  if (status == STATUS_PENDING)
  {
    IoMarkIrpPending(irp);
    IoStartPacket(deviceObject, irp, (ULONG *)NULL, mayuGenericReadCancel);
  }
  else
    IoCompleteRequest(irp, IO_NO_INCREMENT);
  return status;
}


// detour IRP_MJ_WRITE
NTSTATUS detourWrite(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
{
  NTSTATUS status;
  PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(irp);
  ULONG len = irpSp->Parameters.Write.Length;
  CommonData *cd = getCd(deviceObject);
  
  irp->IoStatus.Information = 0;
  if (len == 0)
    status = STATUS_SUCCESS;
  else if (len % sizeof(KEYBOARD_INPUT_DATA))
    status = STATUS_INVALID_PARAMETER;
  else
    // write to filter que
  {
    KIRQL cancelIrql, currentIrql;
    PIRP irpCancel;
    MayuDeviceExtension *devExt = cd->filterDevExt[cd->curkbd];
    
    // enque filter que
    KeAcquireSpinLock(&devExt->lock, &currentIrql);
    IoAcquireCancelSpinLock(&cancelIrql);
    
    len /= sizeof(KEYBOARD_INPUT_DATA);
    len = KqEnque(&devExt->readQue,
		  (KEYBOARD_INPUT_DATA *)irp->AssociatedIrp.SystemBuffer,
		  len);
    irp->IoStatus.Information = len * sizeof(KEYBOARD_INPUT_DATA);
    irpSp->Parameters.Write.Length = irp->IoStatus.Information;
    irpCancel = cd->filterDevExt[cd->curkbd]->kbdClassDevObj->CurrentIrp;
    
    IoReleaseCancelSpinLock(cancelIrql);
    KeReleaseSpinLock(&devExt->lock, currentIrql);

    // cancel filter irp
    if (irpCancel)
      IoCancelIrp(irpCancel);
    
    status = STATUS_SUCCESS;
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
  CommonData *cd = getCd(deviceObject);
  MayuDeviceExtension *devExt =
    (MayuDeviceExtension *)deviceObject->DeviceExtension;
  ULONG i;

  // cancel io
  for (i = 0; i < MAX_KBD; ++i) {
    if (KD_ISSET(cd->kbd_desc, i)) {
	  irpCancel = cd->filterDevExt[i]->kbdClassDevObj->CurrentIrp;
	  if (irpCancel)
		  IoCancelIrp(irpCancel);
	  irpCancel = cd->filterDevObj[i]->CurrentIrp;
	  if (irpCancel)
		  IoCancelIrp(irpCancel);
    }
  }

  KeAcquireSpinLock(&devExt->lock, &currentIrql);
  IoAcquireCancelSpinLock(&cancelIrql);
  irpSp = IoGetCurrentIrpStackLocation(irp);
  devExt->wasCleanupInitiated = TRUE;
  
  // Complete all requests queued by this thread with STATUS_CANCELLED
  currentIrp = deviceObject->CurrentIrp;
  deviceObject->CurrentIrp = NULL;
  devExt->isReadRequestPending = FALSE;
  
  while (currentIrp != NULL)
  {
    IoSetCancelRoutine(currentIrp, NULL);
    currentIrp->IoStatus.Status = STATUS_CANCELLED;
    currentIrp->IoStatus.Information = 0;
    
    IoReleaseCancelSpinLock(cancelIrql);
    KeReleaseSpinLock(&devExt->lock, currentIrql);
    IoCompleteRequest(currentIrp, IO_NO_INCREMENT);
    KeAcquireSpinLock(&devExt->lock, &currentIrql);
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
  KeReleaseSpinLock(&devExt->lock, currentIrql);
  
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
  CommonData *cd = getCd(deviceObject);
  
  irp->IoStatus.Information = 0;
  switch (irpSp->Parameters.DeviceIoControl.IoControlCode)
  {
    case IOCTL_MAYU_DETOUR_CANCEL:
    {
      KIRQL cancelIrql;
      PIRP irpCancel = NULL;
      
      IoAcquireCancelSpinLock(&cancelIrql);
      if (cd->isDetourOpened)
	irpCancel = cd->detourDevObj->CurrentIrp;
      IoReleaseCancelSpinLock(cancelIrql);
      
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
  CommonData *cd = getCd(deviceObject);
  
  cd->curkbd = ((MayuDeviceExtension*)(deviceObject->DeviceExtension))->kbdnum;
  if (cd->isDetourOpened && !cd->detourDevExt->wasCleanupInitiated)
    // read from que
  {
    ULONG len = irpSp->Parameters.Read.Length;
    MayuDeviceExtension *devExt =
      (MayuDeviceExtension *)deviceObject->DeviceExtension;

    irp->IoStatus.Information = 0;
    if (len == 0)
      status = STATUS_SUCCESS;
    else if (len % sizeof(KEYBOARD_INPUT_DATA))
      status = STATUS_BUFFER_TOO_SMALL;
    else if (KqIsEmpty(&devExt->readQue))
      status = STATUS_PENDING;
    else
    {
      KIRQL currentIrql;
      
      KeAcquireSpinLock(&devExt->lock, &currentIrql);
      len = KqDeque(&devExt->readQue,
		    (KEYBOARD_INPUT_DATA *)irp->AssociatedIrp.SystemBuffer,
		    len / sizeof(KEYBOARD_INPUT_DATA));
      KeReleaseSpinLock(&devExt->lock, currentIrql);
	
      irp->IoStatus.Information = len * sizeof(KEYBOARD_INPUT_DATA);
      irpSp->Parameters.Read.Length = irp->IoStatus.Information;
      status = STATUS_SUCCESS;
    }

    if (status != STATUS_PENDING)
    {
      irp->IoStatus.Status = status;
      IoCompleteRequest(irp, IO_NO_INCREMENT);
      return status;
    }
  }
  
  *IoGetNextIrpStackLocation(irp) = *irpSp;
  IoSetCompletionRoutine(irp, filterReadCompletion, NULL, TRUE, TRUE, TRUE);
  return IoCallDriver(getKbdClassDevObj(deviceObject), irp);
}


// pass throught irp to keyboard class driver
NTSTATUS filterPassThrough(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
{
  *IoGetNextIrpStackLocation(irp) = *IoGetCurrentIrpStackLocation(irp);
  IoSetCompletionRoutine(irp, filterGenericCompletion,
			 NULL, TRUE, TRUE, TRUE);
  return IoCallDriver(getKbdClassDevObj(deviceObject), irp);
}


#ifndef MAYUD_NT4
// filter IRP_MJ_POWER
NTSTATUS filterPower(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
{
  PoStartNextPowerIrp(irp);
  IoSkipCurrentIrpStackLocation(irp);
  return PoCallDriver(getKbdClassDevObj(deviceObject), irp);
}
NTSTATUS filterPnP(IN PDEVICE_OBJECT deviceObject, IN PIRP irp)
{
  IoSkipCurrentIrpStackLocation(irp);
  return IoCallDriver(getKbdClassDevObj(deviceObject), irp);
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
#endif // !MAYUD_NT4
