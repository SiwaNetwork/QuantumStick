# QuantumStick Web Monitor - Быстрый старт

## 🚀 Запуск за 2 минуты

### Простейший способ - Демо режим
```bash
cd web_monitor
make demo
```
Откройте браузер: http://localhost:8080

### С реальным устройством
```bash
cd web_monitor
make install
make run
```

## 📋 Пошаговая инструкция

### 1. Подготовка системы
```bash
# Ubuntu/Debian
sudo apt install python3 python3-pip ethtool iproute2

# CentOS/RHEL
sudo yum install python3 python3-pip ethtool iproute
```

### 2. Установка и запуск
```bash
# Переход в директорию
cd web_monitor

# Автоматическая установка
make install

# Проверка устройств
make check

# Запуск мониторинга
make run
```

### 3. Использование веб-интерфейса
1. Откройте браузер: http://localhost:8080
2. Нажмите "Запустить мониторинг"
3. Наблюдайте за данными в реальном времени

## 🎭 Демо режим (без устройства)

Для тестирования интерфейса без реального устройства:

```bash
cd web_monitor
make demo
```

Демо режим предоставляет:
- ✅ Имитацию реальных данных PTP
- ✅ Анимированные графики
- ✅ Случайные алерты
- ✅ Полную функциональность UI

## 🔧 Основные команды

```bash
make help          # Справка по командам
make install       # Установка зависимостей
make run           # Запуск с реальным устройством
make demo          # Запуск в демо режиме
make check         # Проверка системы и устройств
make status        # Проверка статуса
make stop          # Остановка всех процессов
make clean         # Очистка временных файлов
```

## 🌐 Доступ к интерфейсу

- **Локальный**: http://localhost:8080
- **По сети**: http://your-server-ip:8080

## 🚨 Устранение проблем

### Ошибка: "Python не найден"
```bash
# Ubuntu/Debian
sudo apt install python3 python3-pip

# CentOS/RHEL  
sudo yum install python3 python3-pip
```

### Ошибка: "Устройство не найдено"
```bash
# Проверка USB устройств
lsusb | grep -i asix

# Проверка драйверов
make check
```

### Ошибка: "Порт 8080 занят"
```bash
# Остановка процессов
make stop

# Или найти и остановить процесс
sudo netstat -tlnp | grep :8080
sudo kill <PID>
```

## ⚡ Быстрые решения

### Проблема с правами доступа
```bash
# Запуск с sudo (только если необходимо)
sudo make run
```

### Переустановка зависимостей
```bash
make clean-all
make install
```

### Просмотр логов
```bash
make logs
```

## 📱 Мобильное использование

Интерфейс адаптирован для мобильных устройств:
- 📱 Адаптивный дизайн
- 👆 Touch-friendly элементы управления
- 📊 Оптимизированные графики

## 🔗 Полезные ссылки

- [Полная документация](README.md)
- [Исходный код](app.py)
- [Демо режим](demo_mode.py)

---

**Нужна помощь?** Создайте issue в репозитории проекта!