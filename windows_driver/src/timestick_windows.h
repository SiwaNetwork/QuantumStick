#pragma once

#ifndef TIMESTICK_WINDOWS_H
#define TIMESTICK_WINDOWS_H

#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
#include <devguid.h>
#include <usbiodef.h>
#include <usb.h>
#include <initguid.h>

// Константы драйвера
#define TIMESTICK_DEVICE_NAME L"\\Device\\TimeStick"
#define TIMESTICK_DOS_DEVICE_NAME L"\\DosDevices\\TimeStick"
#define TIMESTICK_DEVICE_TYPE 0x8000

// IOCTL коды для Windows
#define IOCTL_TIMESTICK_GET_SIGNATURE \
    CTL_CODE(TIMESTICK_DEVICE_TYPE, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_TIMESTICK_SET_SIGNATURE \
    CTL_CODE(TIMESTICK_DEVICE_TYPE, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_TIMESTICK_USB_COMMAND \
    CTL_CODE(TIMESTICK_DEVICE_TYPE, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_TIMESTICK_GET_STATUS \
    CTL_CODE(TIMESTICK_DEVICE_TYPE, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_TIMESTICK_PTP_CONTROL \
    CTL_CODE(TIMESTICK_DEVICE_TYPE, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_TIMESTICK_GET_STATS \
    CTL_CODE(TIMESTICK_DEVICE_TYPE, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Структуры данных
typedef struct _TIMESTICK_DEVICE_INFO {
    CHAR DriverSignature[32];
    CHAR DeviceVersion[16];
    ULONG LinkSpeed;
    BOOLEAN IsConnected;
    BOOLEAN PtpEnabled;
    LARGE_INTEGER Timestamp;
} TIMESTICK_DEVICE_INFO, *PTIMESTICK_DEVICE_INFO;

typedef struct _TIMESTICK_USB_COMMAND {
    UCHAR Command;
    UCHAR Data[64];
    UCHAR Signature[32];
    ULONG DataLength;
} TIMESTICK_USB_COMMAND, *PTIMESTICK_USB_COMMAND;

typedef struct _TIMESTICK_PTP_STATUS {
    BOOLEAN Enabled;
    LONGLONG CurrentOffsetNs;
    LONGLONG MinOffsetNs;
    LONGLONG MaxOffsetNs;
    LONGLONG AvgOffsetNs;
    ULONG SyncCount;
    DOUBLE FrequencyAdjustmentPpm;
    LARGE_INTEGER LastSyncTime;
} TIMESTICK_PTP_STATUS, *PTIMESTICK_PTP_STATUS;

typedef struct _TIMESTICK_NETWORK_STATS {
    ULONGLONG RxPackets;
    ULONGLONG TxPackets;
    ULONGLONG RxBytes;
    ULONGLONG TxBytes;
    ULONG RxErrors;
    ULONG TxErrors;
    DOUBLE RxRateMbps;
    DOUBLE TxRateMbps;
} TIMESTICK_NETWORK_STATS, *PTIMESTICK_NETWORK_STATS;

// GUID для устройства TimeStick
DEFINE_GUID(GUID_DEVINTERFACE_TIMESTICK,
    0x12345678, 0x1234, 0x5678, 0x90, 0xab, 0xcd, 0xef, 0x12, 0x34, 0x56, 0x78);

// Функции экспорта
#ifdef __cplusplus
extern "C" {
#endif

// Функции для работы с устройством
HANDLE OpenTimeStickDevice(void);
VOID CloseTimeStickDevice(HANDLE hDevice);
BOOLEAN GetDeviceInfo(HANDLE hDevice, PTIMESTICK_DEVICE_INFO pDeviceInfo);
BOOLEAN SendUsbCommand(HANDLE hDevice, PTIMESTICK_USB_COMMAND pCommand);
BOOLEAN GetPtpStatus(HANDLE hDevice, PTIMESTICK_PTP_STATUS pPtpStatus);
BOOLEAN GetNetworkStats(HANDLE hDevice, PTIMESTICK_NETWORK_STATS pStats);
BOOLEAN SetPtpControl(HANDLE hDevice, BOOLEAN Enable);

#ifdef __cplusplus
}
#endif

#endif // TIMESTICK_WINDOWS_H