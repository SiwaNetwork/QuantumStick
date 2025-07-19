#!/usr/bin/env python3
"""
QuantumStick Web Monitor - Demo Mode
Демонстрационный режим для тестирования веб-интерфейса без реального устройства
"""

import os
import sys
import json
import time
import random
import threading
from datetime import datetime, timedelta
from flask import Flask, render_template, request, jsonify
from flask_socketio import SocketIO, emit
import logging

# Настройка логирования
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = Flask(__name__)
app.config['SECRET_KEY'] = 'quantum_stick_demo_secret'
socketio = SocketIO(app, cors_allowed_origins="*")

# Демо данные
demo_data = {
    'device_info': {
        'interface': 'eth0',
        'driver': 'ax88179_178a_demo',
        'version': '1.2.0-demo',
        'connection_status': 'Connected',
        'link_speed': 1000,
        'is_online': True
    },
    'ptp_status': {
        'enabled': True,
        'sync_count': 0,
        'current_offset_ns': 0,
        'min_offset_ns': -500,
        'max_offset_ns': 500,
        'avg_offset_ns': 0,
        'last_sync': None,
        'frequency_adjustment_ppm': 0.0
    },
    'pps_status': {
        'enabled': True,
        'pulse_count': 0,
        'last_pulse_time': None,
        'pulse_interval_ms': 1000.0,
        'pulse_jitter_us': 2.0,
        'min_interval_ms': 999.995,
        'max_interval_ms': 1000.005
    },
    'network_stats': {
        'rx_packets': 1000000,
        'tx_packets': 950000,
        'rx_bytes': 1500000000,
        'tx_bytes': 1200000000,
        'rx_errors': 0,
        'tx_errors': 0,
        'rx_rate_mbps': 0.0,
        'tx_rate_mbps': 0.0
    },
    'system_info': {
        'cpu_usage': 25.0,
        'memory_usage': 45.0,
        'uptime': 86400,
        'temperature': 42.5
    },
    'alerts': [
        {
            'level': 'success',
            'message': 'Демо режим активен - все системы работают нормально',
            'timestamp': datetime.now().isoformat()
        }
    ],
    'history': {
        'ptp_offset': [],
        'network_throughput': [],
        'timestamps': []
    }
}

class DemoMonitor:
    """Демонстрационный монитор для имитации работы с устройством"""
    
    def __init__(self):
        self.running = False
        self.monitor_thread = None
        self.start_time = time.time()
        
    def generate_realistic_data(self):
        """Генерация реалистичных данных для демонстрации"""
        current_time = time.time()
        elapsed = current_time - self.start_time
        
        # Имитация PTP смещения с небольшими колебаниями
        base_offset = 100 * math.sin(elapsed / 60)  # Медленные изменения
        noise = random.randint(-50, 50)  # Шум
        demo_data['ptp_status']['current_offset_ns'] = int(base_offset + noise)
        demo_data['ptp_status']['sync_count'] += 1
        demo_data['ptp_status']['last_sync'] = datetime.now().isoformat()
        
        # Обновление статистики PTP
        offset = demo_data['ptp_status']['current_offset_ns']
        if offset < demo_data['ptp_status']['min_offset_ns']:
            demo_data['ptp_status']['min_offset_ns'] = offset
        if offset > demo_data['ptp_status']['max_offset_ns']:
            demo_data['ptp_status']['max_offset_ns'] = offset
        
        # Имитация сетевого трафика
        base_traffic = 50 + 30 * math.sin(elapsed / 30)  # Mbps
        demo_data['network_stats']['rx_rate_mbps'] = base_traffic + random.uniform(-5, 5)
        demo_data['network_stats']['tx_rate_mbps'] = base_traffic * 0.8 + random.uniform(-3, 3)
        
        # Накопление пакетов и байтов
        demo_data['network_stats']['rx_packets'] += random.randint(100, 1000)
        demo_data['network_stats']['tx_packets'] += random.randint(80, 800)
        demo_data['network_stats']['rx_bytes'] += random.randint(150000, 1500000)
        demo_data['network_stats']['tx_bytes'] += random.randint(120000, 1200000)
        
        # Иногда добавляем ошибки
        if random.random() < 0.01:  # 1% вероятность
            demo_data['network_stats']['rx_errors'] += random.randint(1, 3)
        if random.random() < 0.005:  # 0.5% вероятность
            demo_data['network_stats']['tx_errors'] += random.randint(1, 2)
        
        # Имитация системной нагрузки
        demo_data['system_info']['cpu_usage'] = 20 + 15 * math.sin(elapsed / 45) + random.uniform(-5, 5)
        demo_data['system_info']['cpu_usage'] = max(0, min(100, demo_data['system_info']['cpu_usage']))
        
        demo_data['system_info']['memory_usage'] = 40 + 10 * math.sin(elapsed / 120) + random.uniform(-3, 3)
        demo_data['system_info']['memory_usage'] = max(0, min(100, demo_data['system_info']['memory_usage']))
        
        demo_data['system_info']['uptime'] = int(elapsed)
        demo_data['system_info']['temperature'] = 40 + 5 * math.sin(elapsed / 200) + random.uniform(-2, 2)
        
        # 1PPS статистика
        demo_data['pps_status']['pulse_count'] += 1
        demo_data['pps_status']['last_pulse_time'] = datetime.now().isoformat()
        demo_data['pps_status']['pulse_interval_ms'] = 1000.0 + random.uniform(-0.01, 0.01)
        demo_data['pps_status']['pulse_jitter_us'] = random.uniform(0.5, 3.0)
        
    def update_history(self):
        """Обновление исторических данных"""
        timestamp = datetime.now().isoformat()
        
        # Ограничиваем историю последними 100 точками
        max_history = 100
        
        demo_data['history']['timestamps'].append(timestamp)
        demo_data['history']['ptp_offset'].append(demo_data['ptp_status']['current_offset_ns'])
        demo_data['history']['network_throughput'].append(
            demo_data['network_stats']['rx_rate_mbps'] + demo_data['network_stats']['tx_rate_mbps']
        )
        
        # Обрезаем историю
        for key in demo_data['history']:
            if len(demo_data['history'][key]) > max_history:
                demo_data['history'][key] = demo_data['history'][key][-max_history:]
    
    def generate_alerts(self):
        """Генерация случайных алертов для демонстрации"""
        if random.random() < 0.02:  # 2% вероятность нового алерта
            alert_types = [
                {
                    'level': 'warning',
                    'message': 'Демо: Имитация небольшого PTP смещения'
                },
                {
                    'level': 'success',
                    'message': 'Демо: Система синхронизации стабильна'
                },
                {
                    'level': 'warning',
                    'message': 'Демо: Временный скачок сетевого трафика'
                }
            ]
            
            alert = random.choice(alert_types)
            alert['timestamp'] = datetime.now().isoformat()
            
            demo_data['alerts'].append(alert)
            demo_data['alerts'] = demo_data['alerts'][-10:]  # Последние 10 алертов
    
    def monitor_loop(self):
        """Основной цикл демонстрации"""
        logger.info("Запуск демо режима QuantumStick")
        
        while self.running:
            try:
                # Обновляем демо данные
                self.generate_realistic_data()
                self.update_history()
                self.generate_alerts()
                
                # Отправляем данные всем подключенным клиентам
                socketio.emit('device_update', demo_data)
                
                time.sleep(1)  # Обновление каждую секунду
                
            except Exception as e:
                logger.error(f"Ошибка в демо цикле: {e}")
                time.sleep(5)
    
    def start_monitoring(self):
        """Запуск демо мониторинга"""
        if self.running:
            return False
            
        logger.info("Запуск демо режима QuantumStick Monitor")
        
        self.running = True
        self.monitor_thread = threading.Thread(target=self.monitor_loop)
        self.monitor_thread.daemon = True
        self.monitor_thread.start()
        
        # Инициализация истории демо данными
        for i in range(50):
            self.generate_realistic_data()
            self.update_history()
            time.sleep(0.01)  # Быстрое заполнение истории
        
        return True
    
    def stop_monitoring(self):
        """Остановка демо мониторинга"""
        self.running = False
        if self.monitor_thread:
            self.monitor_thread.join(timeout=5)

# Добавляем математические функции
import math

# Создаем экземпляр демо монитора
demo_monitor = DemoMonitor()

@app.route('/')
def index():
    """Главная страница"""
    return render_template('index.html')

@app.route('/api/device_data')
def get_device_data():
    """API для получения демо данных устройства"""
    return jsonify(demo_data)

@app.route('/api/start_monitoring', methods=['POST'])
def start_monitoring():
    """API для запуска демо мониторинга"""
    if demo_monitor.start_monitoring():
        return jsonify({'status': 'success', 'message': 'Демо мониторинг запущен'})
    else:
        return jsonify({'status': 'error', 'message': 'Демо мониторинг уже запущен'})

@app.route('/api/stop_monitoring', methods=['POST'])
def stop_monitoring():
    """API для остановки демо мониторинга"""
    demo_monitor.stop_monitoring()
    return jsonify({'status': 'success', 'message': 'Демо мониторинг остановлен'})

@socketio.on('connect')
def handle_connect():
    """Обработка подключения клиента"""
    logger.info(f"Клиент подключен к демо режиму: {request.sid}")
    emit('device_update', demo_data)

@socketio.on('disconnect')
def handle_disconnect():
    """Обработка отключения клиента"""
    logger.info(f"Клиент отключен от демо режима: {request.sid}")

@socketio.on('request_data')
def handle_request_data():
    """Обработка запроса данных от клиента"""
    emit('device_update', demo_data)

if __name__ == '__main__':
    print("\n" + "="*60)
    print("        QuantumStick Web Monitor - DEMO MODE")
    print("="*60)
    print("🎭 Демонстрационный режим активен")
    print("📊 Имитация данных от устройства QuantumStick")
    print("🌐 Веб-интерфейс: http://localhost:8080")
    print("⚠️  Это НЕ реальные данные устройства!")
    print("="*60)
    print()
    
    # Автоматический запуск демо мониторинга
    demo_monitor.start_monitoring()
    
    try:
        # Запуск веб-сервера
        socketio.run(app, host='0.0.0.0', port=8080, debug=False)
    except KeyboardInterrupt:
        logger.info("Получен сигнал завершения")
    finally:
        demo_monitor.stop_monitoring()