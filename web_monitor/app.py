#!/usr/bin/env python3
"""
QuantumStick Web Monitor
Современный веб-интерфейс для мониторинга устройства QuantumStick
"""

import os
import sys
import json
import time
import subprocess
import threading
from datetime import datetime, timedelta
from flask import Flask, render_template, request, jsonify
from flask_socketio import SocketIO, emit
import ctypes
import fcntl
import struct
import socket
import logging

# Настройка логирования
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = Flask(__name__)
app.config['SECRET_KEY'] = 'quantum_stick_monitoring_secret'
socketio = SocketIO(app, cors_allowed_origins="*")

# Глобальные переменные для хранения данных
device_data = {
    'device_info': {
        'interface': 'eth0',
        'driver': 'Unknown',
        'version': 'Unknown',
        'connection_status': 'Disconnected',
        'link_speed': 0,
        'is_online': False
    },
    'ptp_status': {
        'enabled': False,
        'sync_count': 0,
        'current_offset_ns': 0,
        'min_offset_ns': 0,
        'max_offset_ns': 0,
        'avg_offset_ns': 0,
        'last_sync': None,
        'frequency_adjustment_ppm': 0.0
    },
    'pps_status': {
        'enabled': False,
        'pulse_count': 0,
        'last_pulse_time': None,
        'pulse_interval_ms': 0.0,
        'pulse_jitter_us': 0.0,
        'min_interval_ms': 0.0,
        'max_interval_ms': 0.0
    },
    'network_stats': {
        'rx_packets': 0,
        'tx_packets': 0,
        'rx_bytes': 0,
        'tx_bytes': 0,
        'rx_errors': 0,
        'tx_errors': 0,
        'rx_rate_mbps': 0.0,
        'tx_rate_mbps': 0.0
    },
    'system_info': {
        'cpu_usage': 0.0,
        'memory_usage': 0.0,
        'uptime': 0,
        'temperature': 0.0
    },
    'alerts': [],
    'history': {
        'ptp_offset': [],
        'network_throughput': [],
        'timestamps': []
    }
}

# Константы для IOCTL
SIOCDEVPRIVATE = 0x89F0
AX_SIGNATURE = 0
AX_USB_COMMAND = 1

class TimeStickMonitor:
    """Класс для мониторинга устройства TimeStick"""
    
    def __init__(self):
        self.interface_name = None
        self.sock_fd = -1
        self.running = False
        self.monitor_thread = None
        
    def find_timestick_interface(self):
        """Поиск интерфейса TimeStick"""
        try:
            # Проверяем доступные сетевые интерфейсы
            result = subprocess.run(['ip', 'link', 'show'], 
                                  capture_output=True, text=True)
            if result.returncode != 0:
                return None
                
            # Ищем интерфейсы ethernet
            interfaces = []
            for line in result.stdout.split('\n'):
                if 'state UP' in line and 'eth' in line:
                    interface = line.split(':')[1].strip()
                    interfaces.append(interface)
            
            # Проверяем каждый интерфейс на наличие драйвера TimeStick
            for interface in interfaces:
                if self.check_timestick_driver(interface):
                    self.interface_name = interface
                    return interface
                    
            return None
            
        except Exception as e:
            logger.error(f"Ошибка поиска интерфейса: {e}")
            return None
    
    def check_timestick_driver(self, interface):
        """Проверка драйвера TimeStick для интерфейса"""
        try:
            result = subprocess.run(['ethtool', '-i', interface], 
                                  capture_output=True, text=True)
            if result.returncode == 0:
                output = result.stdout.lower()
                return 'ax88179' in output or 'ax88279' in output
        except:
            pass
        return False
    
    def get_device_info(self):
        """Получение информации об устройстве"""
        if not self.interface_name:
            return False
            
        try:
            # Получаем информацию о драйвере
            result = subprocess.run(['ethtool', '-i', self.interface_name], 
                                  capture_output=True, text=True)
            if result.returncode == 0:
                lines = result.stdout.split('\n')
                for line in lines:
                    if 'driver:' in line:
                        device_data['device_info']['driver'] = line.split(':')[1].strip()
                    elif 'version:' in line:
                        device_data['device_info']['version'] = line.split(':')[1].strip()
            
            # Получаем скорость соединения
            result = subprocess.run(['ethtool', self.interface_name], 
                                  capture_output=True, text=True)
            if result.returncode == 0:
                for line in result.stdout.split('\n'):
                    if 'Speed:' in line:
                        speed_str = line.split(':')[1].strip()
                        if 'Mb/s' in speed_str:
                            device_data['device_info']['link_speed'] = int(speed_str.replace('Mb/s', ''))
                        elif 'Gb/s' in speed_str:
                            device_data['device_info']['link_speed'] = int(float(speed_str.replace('Gb/s', '')) * 1000)
                    elif 'Link detected:' in line:
                        is_connected = 'yes' in line.lower()
                        device_data['device_info']['connection_status'] = 'Connected' if is_connected else 'Disconnected'
                        device_data['device_info']['is_online'] = is_connected
            
            device_data['device_info']['interface'] = self.interface_name
            return True
            
        except Exception as e:
            logger.error(f"Ошибка получения информации об устройстве: {e}")
            return False
    
    def get_network_stats(self):
        """Получение сетевой статистики"""
        try:
            # Читаем статистику из /proc/net/dev
            with open('/proc/net/dev', 'r') as f:
                lines = f.readlines()
            
            for line in lines:
                if self.interface_name and self.interface_name in line:
                    parts = line.split()
                    if len(parts) >= 17:
                        # RX статистика
                        device_data['network_stats']['rx_bytes'] = int(parts[1])
                        device_data['network_stats']['rx_packets'] = int(parts[2])
                        device_data['network_stats']['rx_errors'] = int(parts[3])
                        
                        # TX статистика  
                        device_data['network_stats']['tx_bytes'] = int(parts[9])
                        device_data['network_stats']['tx_packets'] = int(parts[10])
                        device_data['network_stats']['tx_errors'] = int(parts[11])
                    break
                    
        except Exception as e:
            logger.error(f"Ошибка получения сетевой статистики: {e}")
    
    def get_ptp_status(self):
        """Получение статуса PTP"""
        try:
            # Проверяем наличие PTP устройства
            ptp_devices = []
            for i in range(10):
                ptp_path = f'/dev/ptp{i}'
                if os.path.exists(ptp_path):
                    ptp_devices.append(ptp_path)
            
            if ptp_devices:
                device_data['ptp_status']['enabled'] = True
                # Здесь можно добавить более детальное чтение PTP статистики
                # с использованием PTP API или парсинга вывода chrony/ptp4l
                
                # Имитация данных для демонстрации
                import random
                device_data['ptp_status']['current_offset_ns'] = random.randint(-1000, 1000)
                device_data['ptp_status']['sync_count'] += 1
                device_data['ptp_status']['last_sync'] = datetime.now().isoformat()
            else:
                device_data['ptp_status']['enabled'] = False
                
        except Exception as e:
            logger.error(f"Ошибка получения PTP статуса: {e}")
    
    def get_system_info(self):
        """Получение системной информации"""
        try:
            # CPU загрузка
            with open('/proc/loadavg', 'r') as f:
                load = float(f.read().split()[0])
                device_data['system_info']['cpu_usage'] = min(load * 10, 100)
            
            # Память
            with open('/proc/meminfo', 'r') as f:
                meminfo = f.read()
                for line in meminfo.split('\n'):
                    if 'MemTotal:' in line:
                        total = int(line.split()[1])
                    elif 'MemAvailable:' in line:
                        available = int(line.split()[1])
                        used_percent = ((total - available) / total) * 100
                        device_data['system_info']['memory_usage'] = used_percent
                        break
            
            # Uptime
            with open('/proc/uptime', 'r') as f:
                uptime_seconds = float(f.read().split()[0])
                device_data['system_info']['uptime'] = int(uptime_seconds)
                
            # Температура (если доступна)
            try:
                with open('/sys/class/thermal/thermal_zone0/temp', 'r') as f:
                    temp = int(f.read()) / 1000.0
                    device_data['system_info']['temperature'] = temp
            except:
                device_data['system_info']['temperature'] = 0.0
                
        except Exception as e:
            logger.error(f"Ошибка получения системной информации: {e}")
    
    def update_history(self):
        """Обновление исторических данных"""
        timestamp = datetime.now().isoformat()
        
        # Ограничиваем историю последними 100 точками
        max_history = 100
        
        device_data['history']['timestamps'].append(timestamp)
        device_data['history']['ptp_offset'].append(device_data['ptp_status']['current_offset_ns'])
        device_data['history']['network_throughput'].append(
            device_data['network_stats']['rx_rate_mbps'] + device_data['network_stats']['tx_rate_mbps']
        )
        
        # Обрезаем историю
        for key in device_data['history']:
            if len(device_data['history'][key]) > max_history:
                device_data['history'][key] = device_data['history'][key][-max_history:]
    
    def check_alerts(self):
        """Проверка условий для алертов"""
        alerts = []
        
        # Проверка соединения
        if not device_data['device_info']['is_online']:
            alerts.append({
                'level': 'error',
                'message': 'Устройство не подключено',
                'timestamp': datetime.now().isoformat()
            })
        
        # Проверка PTP смещения
        if device_data['ptp_status']['enabled']:
            offset = abs(device_data['ptp_status']['current_offset_ns'])
            if offset > 10000:  # > 10 мкс
                alerts.append({
                    'level': 'warning',
                    'message': f'Большое PTP смещение: {offset} нс',
                    'timestamp': datetime.now().isoformat()
                })
        
        # Проверка ошибок сети
        if device_data['network_stats']['rx_errors'] > 0 or device_data['network_stats']['tx_errors'] > 0:
            alerts.append({
                'level': 'warning',
                'message': f"Сетевые ошибки: RX={device_data['network_stats']['rx_errors']}, TX={device_data['network_stats']['tx_errors']}",
                'timestamp': datetime.now().isoformat()
            })
        
        device_data['alerts'] = alerts[-10:]  # Последние 10 алертов
    
    def monitor_loop(self):
        """Основной цикл мониторинга"""
        logger.info("Запуск мониторинга QuantumStick")
        
        while self.running:
            try:
                # Обновляем данные
                self.get_device_info()
                self.get_network_stats()
                self.get_ptp_status()
                self.get_system_info()
                self.update_history()
                self.check_alerts()
                
                # Отправляем данные всем подключенным клиентам
                socketio.emit('device_update', device_data)
                
                time.sleep(1)  # Обновление каждую секунду
                
            except Exception as e:
                logger.error(f"Ошибка в цикле мониторинга: {e}")
                time.sleep(5)
    
    def start_monitoring(self):
        """Запуск мониторинга"""
        if self.running:
            return False
            
        # Поиск устройства
        if not self.find_timestick_interface():
            logger.error("Устройство QuantumStick не найдено")
            return False
        
        logger.info(f"Найдено устройство на интерфейсе: {self.interface_name}")
        
        self.running = True
        self.monitor_thread = threading.Thread(target=self.monitor_loop)
        self.monitor_thread.daemon = True
        self.monitor_thread.start()
        
        return True
    
    def stop_monitoring(self):
        """Остановка мониторинга"""
        self.running = False
        if self.monitor_thread:
            self.monitor_thread.join(timeout=5)

# Создаем экземпляр монитора
monitor = TimeStickMonitor()

@app.route('/')
def index():
    """Главная страница"""
    return render_template('index.html')

@app.route('/api/device_data')
def get_device_data():
    """API для получения данных устройства"""
    return jsonify(device_data)

@app.route('/api/start_monitoring', methods=['POST'])
def start_monitoring():
    """API для запуска мониторинга"""
    if monitor.start_monitoring():
        return jsonify({'status': 'success', 'message': 'Мониторинг запущен'})
    else:
        return jsonify({'status': 'error', 'message': 'Не удалось запустить мониторинг'})

@app.route('/api/stop_monitoring', methods=['POST'])
def stop_monitoring():
    """API для остановки мониторинга"""
    monitor.stop_monitoring()
    return jsonify({'status': 'success', 'message': 'Мониторинг остановлен'})

@socketio.on('connect')
def handle_connect():
    """Обработка подключения клиента"""
    logger.info(f"Клиент подключен: {request.sid}")
    emit('device_update', device_data)

@socketio.on('disconnect')
def handle_disconnect():
    """Обработка отключения клиента"""
    logger.info(f"Клиент отключен: {request.sid}")

@socketio.on('request_data')
def handle_request_data():
    """Обработка запроса данных от клиента"""
    emit('device_update', device_data)

if __name__ == '__main__':
    # Автоматический запуск мониторинга при старте
    monitor.start_monitoring()
    
    try:
        # Запуск веб-сервера
        socketio.run(app, host='0.0.0.0', port=8080, debug=False)
    except KeyboardInterrupt:
        logger.info("Получен сигнал завершения")
    finally:
        monitor.stop_monitoring()