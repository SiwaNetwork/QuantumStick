#!/usr/bin/env python3
"""
TimeStick Windows Support Module
Модуль для работы с TimeStick устройствами в Windows
"""

import os
import sys
import json
import ctypes
import time
import subprocess
import threading
from ctypes import wintypes, Structure, c_char, c_ulong, c_bool, c_longlong, c_double
from typing import Dict, List, Optional, Tuple
import logging

# Настройка логирования
logger = logging.getLogger(__name__)

# Определение структур для взаимодействия с драйвером
class TIMESTICK_DEVICE_INFO(Structure):
    _fields_ = [
        ("DriverSignature", c_char * 32),
        ("DeviceVersion", c_char * 16),
        ("LinkSpeed", c_ulong),
        ("IsConnected", c_bool),
        ("PtpEnabled", c_bool),
        ("Timestamp", c_longlong)
    ]

class TIMESTICK_PTP_STATUS(Structure):
    _fields_ = [
        ("Enabled", c_bool),
        ("CurrentOffsetNs", c_longlong),
        ("MinOffsetNs", c_longlong),
        ("MaxOffsetNs", c_longlong),
        ("AvgOffsetNs", c_longlong),
        ("SyncCount", c_ulong),
        ("FrequencyAdjustmentPpm", c_double),
        ("LastSyncTime", c_longlong)
    ]

class TIMESTICK_NETWORK_STATS(Structure):
    _fields_ = [
        ("RxPackets", c_longlong),
        ("TxPackets", c_longlong),
        ("RxBytes", c_longlong),
        ("TxBytes", c_longlong),
        ("RxErrors", c_ulong),
        ("TxErrors", c_ulong),
        ("RxRateMbps", c_double),
        ("TxRateMbps", c_double)
    ]

class WindowsTimeStickMonitor:
    """Класс для мониторинга TimeStick устройств в Windows"""
    
    def __init__(self):
        self.library = None
        self.device_handle = None
        self.is_connected = False
        self.monitor_thread = None
        self.stop_monitoring = False
        self.last_data = {}
        self._load_library()
    
    def _load_library(self):
        """Загрузка пользовательской библиотеки TimeStick"""
        try:
            # Поиск библиотеки в различных местах
            library_paths = [
                "timestick.dll",
                "windows_driver/build/output/usermode/timestick.dll",
                os.path.join(os.path.dirname(__file__), "timestick.dll"),
                os.path.join(os.path.dirname(__file__), "../windows_driver/build/output/usermode/timestick.dll")
            ]
            
            for path in library_paths:
                if os.path.exists(path):
                    self.library = ctypes.CDLL(path)
                    logger.info(f"Загружена библиотека TimeStick: {path}")
                    break
            
            if not self.library:
                logger.warning("Библиотека TimeStick не найдена, используется эмуляция")
                self._setup_emulation()
            else:
                self._setup_library_functions()
                
        except Exception as e:
            logger.error(f"Ошибка загрузки библиотеки: {e}")
            self._setup_emulation()
    
    def _setup_library_functions(self):
        """Настройка функций библиотеки"""
        if not self.library:
            return
            
        try:
            # Настройка типов функций
            self.library.OpenTimeStickDevice.restype = wintypes.HANDLE
            self.library.CloseTimeStickDevice.argtypes = [wintypes.HANDLE]
            self.library.GetDeviceInfo.argtypes = [wintypes.HANDLE, ctypes.POINTER(TIMESTICK_DEVICE_INFO)]
            self.library.GetDeviceInfo.restype = c_bool
            self.library.GetPtpStatus.argtypes = [wintypes.HANDLE, ctypes.POINTER(TIMESTICK_PTP_STATUS)]
            self.library.GetPtpStatus.restype = c_bool
            self.library.GetNetworkStats.argtypes = [wintypes.HANDLE, ctypes.POINTER(TIMESTICK_NETWORK_STATS)]
            self.library.GetNetworkStats.restype = c_bool
            self.library.SetPtpControl.argtypes = [wintypes.HANDLE, c_bool]
            self.library.SetPtpControl.restype = c_bool
            
        except Exception as e:
            logger.error(f"Ошибка настройки функций библиотеки: {e}")
    
    def _setup_emulation(self):
        """Настройка эмуляции для тестирования без драйвера"""
        self.emulation_mode = True
        self.emulation_data = {
            'device_info': {
                'driver_signature': 'AX88279_Windows_v1.0_Emulated',
                'device_version': '2.1.3',
                'link_speed': 2500,
                'is_connected': True,
                'ptp_enabled': False
            },
            'ptp_status': {
                'enabled': False,
                'sync_count': 0,
                'current_offset_ns': 0,
                'frequency_adjustment_ppm': 0.0
            },
            'network_stats': {
                'rx_packets': 1000,
                'tx_packets': 800,
                'rx_bytes': 1500000,
                'tx_bytes': 1200000,
                'rx_errors': 0,
                'tx_errors': 0,
                'rx_rate_mbps': 100.0,
                'tx_rate_mbps': 80.0
            }
        }
    
    def connect(self) -> bool:
        """Подключение к устройству TimeStick"""
        try:
            if hasattr(self, 'emulation_mode'):
                self.is_connected = True
                logger.info("Подключение к устройству TimeStick (эмуляция)")
                return True
            
            if self.library:
                self.device_handle = self.library.OpenTimeStickDevice()
                if self.device_handle and self.device_handle != -1:
                    self.is_connected = True
                    logger.info("Подключение к устройству TimeStick успешно")
                    return True
            
            logger.warning("Не удалось подключиться к устройству TimeStick")
            return False
            
        except Exception as e:
            logger.error(f"Ошибка подключения к устройству: {e}")
            return False
    
    def disconnect(self):
        """Отключение от устройства"""
        try:
            if hasattr(self, 'emulation_mode'):
                self.is_connected = False
                return
            
            if self.library and self.device_handle:
                self.library.CloseTimeStickDevice(self.device_handle)
                self.device_handle = None
                self.is_connected = False
                logger.info("Отключение от устройства TimeStick")
                
        except Exception as e:
            logger.error(f"Ошибка отключения от устройства: {e}")
    
    def get_device_info(self) -> Dict:
        """Получение информации об устройстве"""
        try:
            if hasattr(self, 'emulation_mode'):
                return self.emulation_data['device_info'].copy()
            
            if not self.is_connected or not self.library:
                return {}
            
            device_info = TIMESTICK_DEVICE_INFO()
            if self.library.GetDeviceInfo(self.device_handle, ctypes.byref(device_info)):
                return {
                    'driver_signature': device_info.DriverSignature.decode('utf-8', errors='ignore'),
                    'device_version': device_info.DeviceVersion.decode('utf-8', errors='ignore'),
                    'link_speed': device_info.LinkSpeed,
                    'is_connected': device_info.IsConnected,
                    'ptp_enabled': device_info.PtpEnabled
                }
            
            return {}
            
        except Exception as e:
            logger.error(f"Ошибка получения информации об устройстве: {e}")
            return {}
    
    def get_ptp_status(self) -> Dict:
        """Получение статуса PTP"""
        try:
            if hasattr(self, 'emulation_mode'):
                # Симуляция изменения данных
                import random
                data = self.emulation_data['ptp_status'].copy()
                if data['enabled']:
                    data['sync_count'] += 1
                    data['current_offset_ns'] = random.randint(-500, 500)
                return data
            
            if not self.is_connected or not self.library:
                return {}
            
            ptp_status = TIMESTICK_PTP_STATUS()
            if self.library.GetPtpStatus(self.device_handle, ctypes.byref(ptp_status)):
                return {
                    'enabled': ptp_status.Enabled,
                    'current_offset_ns': ptp_status.CurrentOffsetNs,
                    'min_offset_ns': ptp_status.MinOffsetNs,
                    'max_offset_ns': ptp_status.MaxOffsetNs,
                    'avg_offset_ns': ptp_status.AvgOffsetNs,
                    'sync_count': ptp_status.SyncCount,
                    'frequency_adjustment_ppm': ptp_status.FrequencyAdjustmentPpm
                }
            
            return {}
            
        except Exception as e:
            logger.error(f"Ошибка получения статуса PTP: {e}")
            return {}
    
    def get_network_stats(self) -> Dict:
        """Получение сетевой статистики"""
        try:
            if hasattr(self, 'emulation_mode'):
                # Симуляция изменения статистики
                import random
                data = self.emulation_data['network_stats'].copy()
                data['rx_packets'] += random.randint(10, 50)
                data['tx_packets'] += random.randint(8, 40)
                data['rx_bytes'] += data['rx_packets'] * 1500
                data['tx_bytes'] += data['tx_packets'] * 1500
                return data
            
            if not self.is_connected or not self.library:
                return {}
            
            network_stats = TIMESTICK_NETWORK_STATS()
            if self.library.GetNetworkStats(self.device_handle, ctypes.byref(network_stats)):
                return {
                    'rx_packets': network_stats.RxPackets,
                    'tx_packets': network_stats.TxPackets,
                    'rx_bytes': network_stats.RxBytes,
                    'tx_bytes': network_stats.TxBytes,
                    'rx_errors': network_stats.RxErrors,
                    'tx_errors': network_stats.TxErrors,
                    'rx_rate_mbps': network_stats.RxRateMbps,
                    'tx_rate_mbps': network_stats.TxRateMbps
                }
            
            return {}
            
        except Exception as e:
            logger.error(f"Ошибка получения сетевой статистики: {e}")
            return {}
    
    def set_ptp_enabled(self, enabled: bool) -> bool:
        """Включение/выключение PTP"""
        try:
            if hasattr(self, 'emulation_mode'):
                self.emulation_data['ptp_status']['enabled'] = enabled
                logger.info(f"PTP {'включен' if enabled else 'выключен'} (эмуляция)")
                return True
            
            if not self.is_connected or not self.library:
                return False
            
            result = self.library.SetPtpControl(self.device_handle, enabled)
            if result:
                logger.info(f"PTP {'включен' if enabled else 'выключен'}")
            return result
            
        except Exception as e:
            logger.error(f"Ошибка управления PTP: {e}")
            return False
    
    def get_complete_status(self) -> Dict:
        """Получение полного статуса устройства"""
        try:
            status = {
                'device_info': self.get_device_info(),
                'ptp_status': self.get_ptp_status(),
                'network_stats': self.get_network_stats(),
                'system_info': self._get_system_info(),
                'timestamp': time.time()
            }
            
            self.last_data = status
            return status
            
        except Exception as e:
            logger.error(f"Ошибка получения статуса: {e}")
            return {}
    
    def _get_system_info(self) -> Dict:
        """Получение системной информации Windows"""
        try:
            import psutil
            return {
                'cpu_usage': psutil.cpu_percent(),
                'memory_usage': psutil.virtual_memory().percent,
                'uptime': time.time() - psutil.boot_time(),
                'platform': 'Windows',
                'os_version': sys.platform
            }
        except ImportError:
            # Альтернативный способ без psutil
            return {
                'cpu_usage': 0.0,
                'memory_usage': 0.0,
                'uptime': 0,
                'platform': 'Windows',
                'os_version': sys.platform
            }
    
    def start_monitoring(self, callback=None, interval=1.0):
        """Запуск мониторинга в отдельном потоке"""
        if self.monitor_thread and self.monitor_thread.is_alive():
            return
        
        self.stop_monitoring = False
        self.monitor_thread = threading.Thread(
            target=self._monitor_loop,
            args=(callback, interval)
        )
        self.monitor_thread.daemon = True
        self.monitor_thread.start()
        logger.info("Мониторинг TimeStick запущен")
    
    def stop_monitoring_thread(self):
        """Остановка мониторинга"""
        self.stop_monitoring = True
        if self.monitor_thread:
            self.monitor_thread.join(timeout=2.0)
        logger.info("Мониторинг TimeStick остановлен")
    
    def _monitor_loop(self, callback, interval):
        """Цикл мониторинга"""
        while not self.stop_monitoring:
            try:
                status = self.get_complete_status()
                if callback and status:
                    callback(status)
                time.sleep(interval)
            except Exception as e:
                logger.error(f"Ошибка в цикле мониторинга: {e}")
                time.sleep(interval)

# Утилитные функции для работы с Windows

def detect_timestick_devices() -> List[Dict]:
    """Обнаружение устройств TimeStick в системе"""
    devices = []
    try:
        # Поиск через WMI
        import wmi
        c = wmi.WMI()
        for device in c.Win32_PnPEntity():
            if device.DeviceID and ('VID_0B95' in device.DeviceID):
                devices.append({
                    'device_id': device.DeviceID,
                    'name': device.Name or 'Unknown TimeStick Device',
                    'status': device.Status,
                    'present': device.Present
                })
    except ImportError:
        logger.warning("WMI не доступен, используется альтернативный метод обнаружения")
        # Альтернативный метод через PowerShell
        try:
            result = subprocess.run([
                'powershell', '-Command',
                'Get-PnpDevice | Where-Object {$_.InstanceId -like "*VID_0B95*"} | Select-Object Name, Status, InstanceId'
            ], capture_output=True, text=True, timeout=10)
            
            if result.returncode == 0:
                # Простой парсинг вывода PowerShell
                lines = result.stdout.strip().split('\n')
                for line in lines[3:]:  # Пропускаем заголовки
                    if line.strip():
                        devices.append({
                            'device_id': line.strip(),
                            'name': 'TimeStick Device',
                            'status': 'OK',
                            'present': True
                        })
        except Exception as e:
            logger.error(f"Ошибка обнаружения устройств: {e}")
    
    return devices

def get_windows_network_interfaces() -> List[Dict]:
    """Получение списка сетевых интерфейсов Windows"""
    interfaces = []
    try:
        result = subprocess.run([
            'powershell', '-Command',
            'Get-NetAdapter | Select-Object Name, InterfaceDescription, LinkSpeed, Status | ConvertTo-Json'
        ], capture_output=True, text=True, timeout=15)
        
        if result.returncode == 0:
            data = json.loads(result.stdout)
            if isinstance(data, dict):
                data = [data]
            
            for iface in data:
                interfaces.append({
                    'name': iface.get('Name', ''),
                    'description': iface.get('InterfaceDescription', ''),
                    'link_speed': iface.get('LinkSpeed', 0),
                    'status': iface.get('Status', 'Unknown')
                })
    except Exception as e:
        logger.error(f"Ошибка получения сетевых интерфейсов: {e}")
    
    return interfaces

def install_driver_package(package_path: str) -> bool:
    """Установка пакета драйвера"""
    try:
        if not os.path.exists(package_path):
            logger.error(f"Пакет драйвера не найден: {package_path}")
            return False
        
        # Проверка прав администратора
        if not ctypes.windll.shell32.IsUserAnAdmin():
            logger.error("Для установки драйвера требуются права администратора")
            return False
        
        # Установка через pnputil
        result = subprocess.run([
            'pnputil', '/add-driver', package_path, '/install'
        ], capture_output=True, text=True)
        
        if result.returncode == 0:
            logger.info(f"Драйвер успешно установлен: {package_path}")
            return True
        else:
            logger.error(f"Ошибка установки драйвера: {result.stderr}")
            return False
            
    except Exception as e:
        logger.error(f"Ошибка установки драйвера: {e}")
        return False

# Глобальный экземпляр монитора
_windows_monitor = None

def get_windows_monitor() -> WindowsTimeStickMonitor:
    """Получение глобального экземпляра Windows монитора"""
    global _windows_monitor
    if _windows_monitor is None:
        _windows_monitor = WindowsTimeStickMonitor()
    return _windows_monitor