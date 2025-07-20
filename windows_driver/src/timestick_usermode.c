#include "timestick_windows.h"
#include <stdio.h>
#include <stdlib.h>

// Внутренние переменные
static HANDLE g_hDevice = INVALID_HANDLE_VALUE;

HANDLE OpenTimeStickDevice(void)
{
    HDEVINFO deviceInfoSet;
    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    PSP_DEVICE_INTERFACE_DETAIL_DATA deviceInterfaceDetailData = NULL;
    ULONG length, requiredLength = 0;
    HANDLE hDevice = INVALID_HANDLE_VALUE;

    // Получение списка устройств
    deviceInfoSet = SetupDiGetClassDevs(&GUID_DEVINTERFACE_TIMESTICK,
                                       NULL,
                                       NULL,
                                       DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        return INVALID_HANDLE_VALUE;
    }

    // Поиск первого устройства
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    
    if (!SetupDiEnumDeviceInterfaces(deviceInfoSet,
                                   NULL,
                                   &GUID_DEVINTERFACE_TIMESTICK,
                                   0,
                                   &deviceInterfaceData)) {
        SetupDiDestroyDeviceInfoList(deviceInfoSet);
        return INVALID_HANDLE_VALUE;
    }

    // Получение размера буфера для деталей интерфейса
    SetupDiGetDeviceInterfaceDetail(deviceInfoSet,
                                  &deviceInterfaceData,
                                  NULL,
                                  0,
                                  &requiredLength,
                                  NULL);

    deviceInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(requiredLength);
    if (!deviceInterfaceDetailData) {
        SetupDiDestroyDeviceInfoList(deviceInfoSet);
        return INVALID_HANDLE_VALUE;
    }

    deviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

    // Получение пути к устройству
    if (!SetupDiGetDeviceInterfaceDetail(deviceInfoSet,
                                       &deviceInterfaceData,
                                       deviceInterfaceDetailData,
                                       requiredLength,
                                       &length,
                                       NULL)) {
        free(deviceInterfaceDetailData);
        SetupDiDestroyDeviceInfoList(deviceInfoSet);
        return INVALID_HANDLE_VALUE;
    }

    // Открытие устройства
    hDevice = CreateFile(deviceInterfaceDetailData->DevicePath,
                        GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        NULL,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                        NULL);

    free(deviceInterfaceDetailData);
    SetupDiDestroyDeviceInfoList(deviceInfoSet);

    if (hDevice != INVALID_HANDLE_VALUE) {
        g_hDevice = hDevice;
    }

    return hDevice;
}

VOID CloseTimeStickDevice(HANDLE hDevice)
{
    if (hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(hDevice);
        if (g_hDevice == hDevice) {
            g_hDevice = INVALID_HANDLE_VALUE;
        }
    }
}

BOOLEAN GetDeviceInfo(HANDLE hDevice, PTIMESTICK_DEVICE_INFO pDeviceInfo)
{
    DWORD bytesReturned;
    BOOLEAN result;

    if (hDevice == INVALID_HANDLE_VALUE || !pDeviceInfo) {
        return FALSE;
    }

    result = DeviceIoControl(hDevice,
                            IOCTL_TIMESTICK_GET_STATUS,
                            NULL,
                            0,
                            pDeviceInfo,
                            sizeof(TIMESTICK_DEVICE_INFO),
                            &bytesReturned,
                            NULL);

    return result && (bytesReturned == sizeof(TIMESTICK_DEVICE_INFO));
}

BOOLEAN SendUsbCommand(HANDLE hDevice, PTIMESTICK_USB_COMMAND pCommand)
{
    DWORD bytesReturned;
    BOOLEAN result;

    if (hDevice == INVALID_HANDLE_VALUE || !pCommand) {
        return FALSE;
    }

    result = DeviceIoControl(hDevice,
                            IOCTL_TIMESTICK_USB_COMMAND,
                            pCommand,
                            sizeof(TIMESTICK_USB_COMMAND),
                            pCommand,
                            sizeof(TIMESTICK_USB_COMMAND),
                            &bytesReturned,
                            NULL);

    return result;
}

BOOLEAN GetPtpStatus(HANDLE hDevice, PTIMESTICK_PTP_STATUS pPtpStatus)
{
    DWORD bytesReturned;
    BOOLEAN result;

    if (hDevice == INVALID_HANDLE_VALUE || !pPtpStatus) {
        return FALSE;
    }

    // Для демонстрации используем GET_STATUS и извлекаем PTP информацию
    TIMESTICK_DEVICE_INFO deviceInfo;
    result = DeviceIoControl(hDevice,
                            IOCTL_TIMESTICK_GET_STATUS,
                            NULL,
                            0,
                            &deviceInfo,
                            sizeof(TIMESTICK_DEVICE_INFO),
                            &bytesReturned,
                            NULL);

    if (result) {
        // Заполняем PTP статус на основе информации об устройстве
        pPtpStatus->Enabled = deviceInfo.PtpEnabled;
        // Остальные поля заполняются драйвером
    }

    return result;
}

BOOLEAN GetNetworkStats(HANDLE hDevice, PTIMESTICK_NETWORK_STATS pStats)
{
    DWORD bytesReturned;
    BOOLEAN result;

    if (hDevice == INVALID_HANDLE_VALUE || !pStats) {
        return FALSE;
    }

    result = DeviceIoControl(hDevice,
                            IOCTL_TIMESTICK_GET_STATS,
                            NULL,
                            0,
                            pStats,
                            sizeof(TIMESTICK_NETWORK_STATS),
                            &bytesReturned,
                            NULL);

    return result && (bytesReturned == sizeof(TIMESTICK_NETWORK_STATS));
}

BOOLEAN SetPtpControl(HANDLE hDevice, BOOLEAN Enable)
{
    DWORD bytesReturned;
    BOOLEAN result;

    if (hDevice == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    result = DeviceIoControl(hDevice,
                            IOCTL_TIMESTICK_PTP_CONTROL,
                            &Enable,
                            sizeof(BOOLEAN),
                            NULL,
                            0,
                            &bytesReturned,
                            NULL);

    return result;
}

// Дополнительные утилитные функции

BOOLEAN IsTimeStickConnected(void)
{
    HANDLE hDevice;
    TIMESTICK_DEVICE_INFO deviceInfo;
    BOOLEAN result = FALSE;

    hDevice = OpenTimeStickDevice();
    if (hDevice != INVALID_HANDLE_VALUE) {
        if (GetDeviceInfo(hDevice, &deviceInfo)) {
            result = deviceInfo.IsConnected;
        }
        CloseTimeStickDevice(hDevice);
    }

    return result;
}

BOOLEAN GetDriverSignature(CHAR* signature, ULONG bufferSize)
{
    HANDLE hDevice;
    DWORD bytesReturned;
    BOOLEAN result = FALSE;

    if (!signature || bufferSize < 32) {
        return FALSE;
    }

    hDevice = OpenTimeStickDevice();
    if (hDevice != INVALID_HANDLE_VALUE) {
        result = DeviceIoControl(hDevice,
                                IOCTL_TIMESTICK_GET_SIGNATURE,
                                NULL,
                                0,
                                signature,
                                min(bufferSize, 32),
                                &bytesReturned,
                                NULL);
        CloseTimeStickDevice(hDevice);
    }

    return result;
}

// Функция для получения всей информации об устройстве
BOOLEAN GetCompleteDeviceStatus(PTIMESTICK_DEVICE_INFO pDeviceInfo,
                               PTIMESTICK_PTP_STATUS pPtpStatus,
                               PTIMESTICK_NETWORK_STATS pNetworkStats)
{
    HANDLE hDevice;
    BOOLEAN result = FALSE;

    hDevice = OpenTimeStickDevice();
    if (hDevice != INVALID_HANDLE_VALUE) {
        BOOLEAN deviceInfoOk = TRUE;
        BOOLEAN ptpStatusOk = TRUE;
        BOOLEAN networkStatsOk = TRUE;

        if (pDeviceInfo) {
            deviceInfoOk = GetDeviceInfo(hDevice, pDeviceInfo);
        }

        if (pPtpStatus) {
            ptpStatusOk = GetPtpStatus(hDevice, pPtpStatus);
        }

        if (pNetworkStats) {
            networkStatsOk = GetNetworkStats(hDevice, pNetworkStats);
        }

        result = deviceInfoOk && ptpStatusOk && networkStatsOk;
        CloseTimeStickDevice(hDevice);
    }

    return result;
}