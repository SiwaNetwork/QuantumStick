#!/bin/bash

# QuantumStick Web Monitor - Сценарий запуска
# Современный веб-интерфейс для мониторинга устройства QuantumStick

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR="$SCRIPT_DIR/venv"
LOG_DIR="$SCRIPT_DIR/logs"

# Цвета для вывода
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Функция логирования
log() {
    echo -e "${BLUE}[$(date '+%Y-%m-%d %H:%M:%S')]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[$(date '+%Y-%m-%d %H:%M:%S')] ✓${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[$(date '+%Y-%m-%d %H:%M:%S')] ⚠${NC} $1"
}

log_error() {
    echo -e "${RED}[$(date '+%Y-%m-%d %H:%M:%S')] ✗${NC} $1"
}

# Функция проверки root прав
check_root() {
    if [[ $EUID -eq 0 ]]; then
        log_warning "Запуск под root. Рекомендуется использовать обычного пользователя."
        read -p "Продолжить? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            exit 1
        fi
    fi
}

# Проверка системных требований
check_requirements() {
    log "Проверка системных требований..."
    
    # Проверка Python
    if ! command -v python3 &> /dev/null; then
        log_error "Python 3 не найден. Установите Python 3.8 или новее."
        exit 1
    fi
    
    PYTHON_VERSION=$(python3 -c 'import sys; print(".".join(map(str, sys.version_info[:2])))')
    log "Найден Python $PYTHON_VERSION"
    
    # Проверка pip
    if ! command -v pip3 &> /dev/null; then
        log_error "pip3 не найден. Установите pip для Python 3."
        exit 1
    fi
    
    # Проверка ethtool
    if ! command -v ethtool &> /dev/null; then
        log_warning "ethtool не найден. Установите ethtool для получения информации о сетевых интерфейсах."
        log "Ubuntu/Debian: sudo apt install ethtool"
        log "CentOS/RHEL: sudo yum install ethtool"
    fi
    
    # Проверка ip
    if ! command -v ip &> /dev/null; then
        log_error "iproute2 не найден. Установите iproute2."
        exit 1
    fi
    
    log_success "Системные требования выполнены"
}

# Настройка виртуального окружения
setup_venv() {
    log "Настройка виртуального окружения..."
    
    if [ ! -d "$VENV_DIR" ]; then
        log "Создание виртуального окружения..."
        python3 -m venv "$VENV_DIR"
    fi
    
    source "$VENV_DIR/bin/activate"
    
    log "Обновление pip..."
    pip install --upgrade pip
    
    log "Установка зависимостей..."
    pip install -r "$SCRIPT_DIR/requirements.txt"
    
    log_success "Виртуальное окружение настроено"
}

# Проверка устройства TimeStick
check_timestick() {
    log "Поиск устройства QuantumStick/TimeStick..."
    
    # Проверка USB устройств
    if command -v lsusb &> /dev/null; then
        ASIX_DEVICES=$(lsusb | grep -i "asix\|ax88" || true)
        if [ -n "$ASIX_DEVICES" ]; then
            log_success "Найдены ASIX USB устройства:"
            echo "$ASIX_DEVICES" | while read line; do
                log "  $line"
            done
        else
            log_warning "ASIX USB устройства не найдены"
        fi
    fi
    
    # Проверка сетевых интерфейсов
    log "Проверка сетевых интерфейсов с драйвером AX88179/AX88279..."
    
    FOUND_INTERFACES=0
    for interface in $(ip link show | grep -E '^[0-9]+:' | awk -F': ' '{print $2}' | grep -E '^eth|^enp'); do
        if command -v ethtool &> /dev/null; then
            DRIVER=$(ethtool -i "$interface" 2>/dev/null | grep "driver:" | awk '{print $2}' || echo "unknown")
            if [[ "$DRIVER" =~ ax88179|ax88279 ]]; then
                log_success "Найден интерфейс: $interface (драйвер: $DRIVER)"
                FOUND_INTERFACES=$((FOUND_INTERFACES + 1))
            fi
        fi
    done
    
    if [ $FOUND_INTERFACES -eq 0 ]; then
        log_warning "Интерфейсы с драйвером TimeStick не найдены"
        log "Это может быть нормально, если устройство не подключено или использует другой драйвер"
    fi
}

# Создание директорий
create_directories() {
    log "Создание рабочих директорий..."
    
    mkdir -p "$LOG_DIR"
    mkdir -p "$SCRIPT_DIR/static"
    mkdir -p "$SCRIPT_DIR/templates"
    
    log_success "Директории созданы"
}

# Запуск веб-сервера
start_server() {
    log "Запуск QuantumStick Web Monitor..."
    
    source "$VENV_DIR/bin/activate"
    
    cd "$SCRIPT_DIR"
    
    # Создание файла логов
    LOG_FILE="$LOG_DIR/quantum_monitor_$(date '+%Y%m%d_%H%M%S').log"
    
    log "Логи сохраняются в: $LOG_FILE"
    log "Веб-интерфейс будет доступен по адресу: http://localhost:8080"
    log_success "Для остановки нажмите Ctrl+C"
    
    echo
    echo "=========================================="
    echo "  QuantumStick Web Monitor запущен"
    echo "=========================================="
    echo "  URL: http://localhost:8080"
    echo "  Логи: $LOG_FILE"
    echo "=========================================="
    echo
    
    # Запуск с логированием
    python3 app.py 2>&1 | tee "$LOG_FILE"
}

# Функция остановки
cleanup() {
    log_warning "Получен сигнал завершения..."
    
    # Остановка фоновых процессов
    jobs -p | xargs -r kill
    
    log_success "QuantumStick Web Monitor остановлен"
    exit 0
}

# Обработка сигналов
trap cleanup SIGINT SIGTERM

# Показ справки
show_help() {
    echo "QuantumStick Web Monitor - Сценарий запуска"
    echo
    echo "Использование: $0 [опции]"
    echo
    echo "Опции:"
    echo "  -h, --help     Показать справку"
    echo "  --setup-only   Только настройка окружения, без запуска"
    echo "  --check-only   Только проверка системы"
    echo "  --no-check     Пропустить проверки"
    echo
    echo "Примеры:"
    echo "  $0                 Полная настройка и запуск"
    echo "  $0 --setup-only    Только настройка виртуального окружения"
    echo "  $0 --check-only    Только проверка системы"
    echo
}

# Основная функция
main() {
    # Обработка аргументов
    SETUP_ONLY=false
    CHECK_ONLY=false
    NO_CHECK=false
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_help
                exit 0
                ;;
            --setup-only)
                SETUP_ONLY=true
                shift
                ;;
            --check-only)
                CHECK_ONLY=true
                shift
                ;;
            --no-check)
                NO_CHECK=true
                shift
                ;;
            *)
                log_error "Неизвестная опция: $1"
                show_help
                exit 1
                ;;
        esac
    done
    
    # Заголовок
    echo
    echo "================================================================"
    echo "           QuantumStick Web Monitor v1.0"
    echo "        Современный веб-интерфейс мониторинга"
    echo "================================================================"
    echo
    
    # Проверка root
    if [ "$NO_CHECK" != true ]; then
        check_root
    fi
    
    # Проверка только системы
    if [ "$CHECK_ONLY" = true ]; then
        check_requirements
        check_timestick
        exit 0
    fi
    
    # Проверки
    if [ "$NO_CHECK" != true ]; then
        check_requirements
        check_timestick
    fi
    
    # Создание директорий
    create_directories
    
    # Настройка окружения
    setup_venv
    
    # Только настройка
    if [ "$SETUP_ONLY" = true ]; then
        log_success "Настройка завершена. Для запуска используйте: $0"
        exit 0
    fi
    
    # Запуск сервера
    start_server
}

# Запуск основной функции
main "$@"