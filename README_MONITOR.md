# TimeStick Monitor - Программы мониторинга устройства

Набор программ для мониторинга устройства TimeStick на HOST компьютере с поддержкой всех функций драйвера, включая PTP синхронизацию и сигнал 1PPS.

## Описание программ

### 1. timestick_monitor
Основная программа мониторинга с графическим интерфейсом (ncurses). Отображает:
- Информацию об устройстве (интерфейс, драйвер, версия)
- Статус соединения и скорость
- Статус PTP синхронизации
- Статус сигнала 1PPS
- Сетевую статистику в реальном времени

### 2. timestick_ptp_monitor
Расширенная программа для детального мониторинга PTP и 1PPS:
- Детальная статистика PTP (смещение, джиттер, синхронизация)
- Мониторинг сигнала 1PPS (интервалы, джиттер)
- Логирование событий
- Экспорт статистики в CSV

### 3. timestick_control
Утилита управления функциями PTP и 1PPS:
- Включение/выключение генерации сигнала 1PPS
- Чтение и установка PTP времени
- Синхронизация PTP с системным временем
- Настройка частотной коррекции PTP (addend)
- Непрерывный мониторинг PTP времени

## Требования

- Linux kernel с поддержкой PTP
- Установленный драйвер TimeStick (ax_usb_nic)
- Библиотеки: ncurses, pthread, math, rt
- Права root для доступа к IOCTL

## Установка

1. Компиляция программ:
```bash
make
```

2. Установка в систему:
```bash
sudo make install
```

3. Для удаления:
```bash
sudo make uninstall
```

## Использование

### timestick_monitor

Запуск с графическим интерфейсом:
```bash
sudo timestick_monitor
```

Управление:
- `q` - выход из программы
- `r` - принудительное обновление данных

### timestick_ptp_monitor

Запуск с опциями:
```bash
# Базовый запуск
sudo timestick_ptp_monitor

# С логированием
sudo timestick_ptp_monitor -l

# Показать помощь
timestick_ptp_monitor -h
```

Команды во время работы:
- `s` - показать статистику
- `c` - сохранить статистику в CSV файл
- `r` - сбросить статистику
- `q` - выход

### timestick_control

Утилита командной строки для управления устройством:
```bash
# Показать информацию об устройстве
sudo timestick_control -i

# Включить генерацию 1PPS
sudo timestick_control -p on

# Выключить генерацию 1PPS
sudo timestick_control -p off

# Получить текущее PTP время
sudo timestick_control -t

# Синхронизировать PTP с системным временем
sudo timestick_control -s

# Установить частотную коррекцию
sudo timestick_control -a 0xCCCCCCCC

# Непрерывный мониторинг PTP времени
sudo timestick_control -m
```

## Функциональность драйвера

Программы используют следующие возможности драйвера TimeStick:

### 1. IOCTL команды
- `AX_SIGNATURE` - получение сигнатуры драйвера
- `AX_USB_COMMAND` - отправка USB команд устройству
- `AX88179A_READ_VERSION` - чтение версии прошивки

### 2. PTP функции
- Чтение локальных PTP часов
- Установка времени PTP
- Контроль частоты (addend)
- Синхронизация с системным временем

### 3. 1PPS функции
- Включение/выключение генерации 1PPS
- Мониторинг импульсов
- Измерение точности и стабильности

## Примеры вывода

### timestick_monitor
```
TimeStick Device Monitor v1.0
═══════════════════════════════════════════════════════════════════

Device Information:
  Interface: eth2
  Driver: AX88179B_179A_772E_772D
  Version: v1.2.0

Connection Status:
  Status: Connected
  Speed: 2500 Mbps

PTP Status:
  PTP: Enabled
  Offset: -125 ns

1PPS Status:
  1PPS: Active
  Last Pulse: 2024-01-15 14:23:45.000000125

Network Statistics:
  RX: 1234567 packets (987654321 bytes)
  TX: 234567 packets (123456789 bytes)
  Errors: RX=0 TX=0

═══════════════════════════════════════════════════════════════════
Press 'q' to quit, 'r' to refresh
```

### timestick_ptp_monitor
```
=== TimeStick PTP/1PPS Monitor ===
Interface: eth2
Driver: AX88179B_179A_772E_772D
PTP Device: /dev/ptp0

PTP Statistics:
  Sync Count: 1000
  Current Offset: -125 ns
  Min Offset: -500 ns
  Max Offset: 250 ns
  Avg Offset: -100 ns
  Last Sync: 2024-01-15 14:23:45

1PPS Statistics:
  Pulse Count: 3600
  Interval: 1000.002 ms
  Jitter: 2.0 us
  Min Interval: 999.998 ms
  Max Interval: 1000.003 ms
```

## Файлы логов и статистики

- `timestick_monitor.log` - лог файл с событиями (при запуске с -l)
- `timestick_stats_YYYYMMDD_HHMMSS.csv` - экспортированная статистика

## Устранение неполадок

### Устройство не найдено
1. Проверьте подключение устройства: `lsusb | grep -i asix`
2. Проверьте загрузку драйвера: `lsmod | grep ax_usb_nic`
3. Загрузите драйвер: `sudo modprobe ax_usb_nic`

### Ошибка доступа к IOCTL
- Запускайте программы с правами root (sudo)

### PTP устройство не найдено
1. Проверьте поддержку PTP в ядре: `ls /dev/ptp*`
2. Проверьте конфигурацию драйвера

## Дополнительные возможности

### Интеграция с chrony/ntpd
Устройство TimeStick с PTP может использоваться как источник точного времени для chrony или ntpd.

### Мониторинг через SNMP
Статистику можно экспортировать для мониторинга через SNMP агенты.

### Автоматизация
Программы поддерживают работу в фоновом режиме для автоматического сбора статистики.

## Лицензия

Программы распространяются под лицензией GPL v2, совместимой с драйвером TimeStick.

## Контакты и поддержка

При возникновении проблем обращайтесь к документации драйвера TimeStick или создайте issue в репозитории проекта.