#include <ntddk.h>
#include <wdf.h>
#include <usb.h>
#include <usbdlib.h>
#include "timestick_windows.h"

// Тег для памяти
#define TIMESTICK_POOL_TAG 'TStk'

// Структура контекста устройства
typedef struct _DEVICE_CONTEXT {
    WDFUSBDEVICE UsbDevice;
    WDFUSBINTERFACE UsbInterface;
    WDFUSBPIPE BulkReadPipe;
    WDFUSBPIPE BulkWritePipe;
    TIMESTICK_DEVICE_INFO DeviceInfo;
    TIMESTICK_PTP_STATUS PtpStatus;
    TIMESTICK_NETWORK_STATS NetworkStats;
    KSPIN_LOCK DataLock;
    KTIMER MonitorTimer;
    KDPC MonitorDpc;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetDeviceContext)

// Прототипы функций
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD TimeStickEvtDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE TimeStickEvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE TimeStickEvtDeviceReleaseHardware;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL TimeStickEvtIoDeviceControl;
EVT_WDF_TIMER TimeStickMonitorTimer;

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;

    WDF_DRIVER_CONFIG_INIT(&config, TimeStickEvtDeviceAdd);

    status = WdfDriverCreate(DriverObject,
                           RegistryPath,
                           WDF_NO_OBJECT_ATTRIBUTES,
                           &config,
                           WDF_NO_HANDLE);

    if (!NT_SUCCESS(status)) {
        KdPrint(("TimeStick: WdfDriverCreate failed with status 0x%x\n", status));
    }

    return status;
}

NTSTATUS
TimeStickEvtDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    NTSTATUS status;
    WDFDEVICE device;
    PDEVICE_CONTEXT deviceContext;
    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDFQUEUE queue;

    UNREFERENCED_PARAMETER(Driver);

    // Настройка PnP callbacks
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
    pnpPowerCallbacks.EvtDevicePrepareHardware = TimeStickEvtDevicePrepareHardware;
    pnpPowerCallbacks.EvtDeviceReleaseHardware = TimeStickEvtDeviceReleaseHardware;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

    // Создание устройства
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);
    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    deviceContext = GetDeviceContext(device);
    KeInitializeSpinLock(&deviceContext->DataLock);

    // Инициализация таймера мониторинга
    WDF_TIMER_CONFIG timerConfig;
    WDF_TIMER_CONFIG_INIT(&timerConfig, TimeStickMonitorTimer);
    timerConfig.Period = 1000; // 1 секунда

    WDF_OBJECT_ATTRIBUTES timerAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes);
    timerAttributes.ParentObject = device;

    status = WdfTimerCreate(&timerConfig, &timerAttributes, &deviceContext->MonitorTimer);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Создание IO очереди
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);
    queueConfig.EvtIoDeviceControl = TimeStickEvtIoDeviceControl;

    status = WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Создание интерфейса устройства
    status = WdfDeviceCreateDeviceInterface(device, &GUID_DEVINTERFACE_TIMESTICK, NULL);

    return status;
}

NTSTATUS
TimeStickEvtDevicePrepareHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourceList,
    _In_ WDFCMRESLIST ResourceListTranslated
)
{
    NTSTATUS status;
    PDEVICE_CONTEXT deviceContext;
    WDF_USB_DEVICE_CREATE_CONFIG createParams;

    UNREFERENCED_PARAMETER(ResourceList);
    UNREFERENCED_PARAMETER(ResourceListTranslated);

    deviceContext = GetDeviceContext(Device);

    // Создание USB устройства
    WDF_USB_DEVICE_CREATE_CONFIG_INIT(&createParams, USBD_CLIENT_CONTRACT_VERSION_602);

    status = WdfUsbTargetDeviceCreateWithParameters(Device,
                                                  &createParams,
                                                  WDF_NO_OBJECT_ATTRIBUTES,
                                                  &deviceContext->UsbDevice);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Настройка USB конфигурации
    status = WdfUsbTargetDeviceSelectConfig(deviceContext->UsbDevice,
                                          WDF_NO_OBJECT_ATTRIBUTES,
                                          NULL);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Получение интерфейса
    deviceContext->UsbInterface = WdfUsbTargetDeviceGetInterface(deviceContext->UsbDevice, 0);

    // Поиск Bulk пайпов
    UCHAR numPipes = WdfUsbInterfaceGetNumEndpoints(deviceContext->UsbInterface, 0);
    
    for (UCHAR i = 0; i < numPipes; i++) {
        WDFUSBPIPE pipe = WdfUsbInterfaceGetConfiguredPipe(deviceContext->UsbInterface, 0, i);
        WDF_USB_PIPE_INFORMATION pipeInfo;
        WDF_USB_PIPE_INFORMATION_INIT(&pipeInfo);
        WdfUsbTargetPipeGetInformation(pipe, &pipeInfo);

        if (WdfUsbPipeTypeBulk == pipeInfo.PipeType) {
            if (WdfUsbTargetPipeIsInEndpoint(pipe)) {
                deviceContext->BulkReadPipe = pipe;
            } else if (WdfUsbTargetPipeIsOutEndpoint(pipe)) {
                deviceContext->BulkWritePipe = pipe;
            }
        }
    }

    // Инициализация информации об устройстве
    RtlZeroMemory(&deviceContext->DeviceInfo, sizeof(TIMESTICK_DEVICE_INFO));
    strcpy_s(deviceContext->DeviceInfo.DriverSignature, 
             sizeof(deviceContext->DeviceInfo.DriverSignature), 
             "AX88279_Windows_v1.0");
    strcpy_s(deviceContext->DeviceInfo.DeviceVersion, 
             sizeof(deviceContext->DeviceInfo.DeviceVersion), 
             "2.1.3");
    deviceContext->DeviceInfo.IsConnected = TRUE;
    KeQuerySystemTime(&deviceContext->DeviceInfo.Timestamp);

    // Запуск таймера мониторинга
    WdfTimerStart(deviceContext->MonitorTimer, WDF_REL_TIMEOUT_IN_MS(1000));

    return status;
}

NTSTATUS
TimeStickEvtDeviceReleaseHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourceListTranslated
)
{
    PDEVICE_CONTEXT deviceContext;

    UNREFERENCED_PARAMETER(ResourceListTranslated);

    deviceContext = GetDeviceContext(Device);

    // Остановка таймера
    WdfTimerStop(deviceContext->MonitorTimer, TRUE);

    return STATUS_SUCCESS;
}

VOID
TimeStickEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
)
{
    NTSTATUS status = STATUS_SUCCESS;
    WDFDEVICE device;
    PDEVICE_CONTEXT deviceContext;
    PVOID inputBuffer = NULL;
    PVOID outputBuffer = NULL;
    size_t bytesReturned = 0;

    device = WdfIoQueueGetDevice(Queue);
    deviceContext = GetDeviceContext(device);

    switch (IoControlCode) {
    case IOCTL_TIMESTICK_GET_STATUS:
        if (OutputBufferLength >= sizeof(TIMESTICK_DEVICE_INFO)) {
            status = WdfRequestRetrieveOutputBuffer(Request, 
                                                  sizeof(TIMESTICK_DEVICE_INFO),
                                                  &outputBuffer, 
                                                  NULL);
            if (NT_SUCCESS(status)) {
                KIRQL oldIrql;
                KeAcquireSpinLock(&deviceContext->DataLock, &oldIrql);
                RtlCopyMemory(outputBuffer, &deviceContext->DeviceInfo, sizeof(TIMESTICK_DEVICE_INFO));
                KeReleaseSpinLock(&deviceContext->DataLock, oldIrql);
                bytesReturned = sizeof(TIMESTICK_DEVICE_INFO);
            }
        } else {
            status = STATUS_BUFFER_TOO_SMALL;
        }
        break;

    case IOCTL_TIMESTICK_GET_SIGNATURE:
        if (OutputBufferLength >= 32) {
            status = WdfRequestRetrieveOutputBuffer(Request, 32, &outputBuffer, NULL);
            if (NT_SUCCESS(status)) {
                KIRQL oldIrql;
                KeAcquireSpinLock(&deviceContext->DataLock, &oldIrql);
                RtlCopyMemory(outputBuffer, deviceContext->DeviceInfo.DriverSignature, 32);
                KeReleaseSpinLock(&deviceContext->DataLock, oldIrql);
                bytesReturned = 32;
            }
        } else {
            status = STATUS_BUFFER_TOO_SMALL;
        }
        break;

    case IOCTL_TIMESTICK_PTP_CONTROL:
        if (InputBufferLength >= sizeof(BOOLEAN)) {
            status = WdfRequestRetrieveInputBuffer(Request, sizeof(BOOLEAN), &inputBuffer, NULL);
            if (NT_SUCCESS(status)) {
                BOOLEAN enable = *(PBOOLEAN)inputBuffer;
                KIRQL oldIrql;
                KeAcquireSpinLock(&deviceContext->DataLock, &oldIrql);
                deviceContext->PtpStatus.Enabled = enable;
                deviceContext->DeviceInfo.PtpEnabled = enable;
                KeReleaseSpinLock(&deviceContext->DataLock, oldIrql);
            }
        } else {
            status = STATUS_BUFFER_TOO_SMALL;
        }
        break;

    case IOCTL_TIMESTICK_GET_STATS:
        if (OutputBufferLength >= sizeof(TIMESTICK_NETWORK_STATS)) {
            status = WdfRequestRetrieveOutputBuffer(Request, 
                                                  sizeof(TIMESTICK_NETWORK_STATS),
                                                  &outputBuffer, 
                                                  NULL);
            if (NT_SUCCESS(status)) {
                KIRQL oldIrql;
                KeAcquireSpinLock(&deviceContext->DataLock, &oldIrql);
                RtlCopyMemory(outputBuffer, &deviceContext->NetworkStats, sizeof(TIMESTICK_NETWORK_STATS));
                KeReleaseSpinLock(&deviceContext->DataLock, oldIrql);
                bytesReturned = sizeof(TIMESTICK_NETWORK_STATS);
            }
        } else {
            status = STATUS_BUFFER_TOO_SMALL;
        }
        break;

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    WdfRequestCompleteWithInformation(Request, status, bytesReturned);
}

VOID
TimeStickMonitorTimer(
    _In_ WDFTIMER Timer
)
{
    WDFDEVICE device;
    PDEVICE_CONTEXT deviceContext;
    KIRQL oldIrql;

    device = WdfTimerGetParentObject(Timer);
    deviceContext = GetDeviceContext(device);

    // Обновление статистики (симуляция)
    KeAcquireSpinLock(&deviceContext->DataLock, &oldIrql);
    
    // Обновление времени
    KeQuerySystemTime(&deviceContext->DeviceInfo.Timestamp);
    
    // Симуляция PTP статистики
    if (deviceContext->PtpStatus.Enabled) {
        deviceContext->PtpStatus.SyncCount++;
        deviceContext->PtpStatus.CurrentOffsetNs = (LONGLONG)(rand() % 1000 - 500);
        KeQuerySystemTime(&deviceContext->PtpStatus.LastSyncTime);
    }
    
    // Симуляция сетевой статистики
    deviceContext->NetworkStats.RxPackets += 100 + (rand() % 50);
    deviceContext->NetworkStats.TxPackets += 80 + (rand() % 40);
    deviceContext->NetworkStats.RxBytes += 1500 * (100 + (rand() % 50));
    deviceContext->NetworkStats.TxBytes += 1500 * (80 + (rand() % 40));
    
    KeReleaseSpinLock(&deviceContext->DataLock, oldIrql);
}