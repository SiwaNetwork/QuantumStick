# QuantumStick Web Monitor - Makefile
# Управление проектом веб-мониторинга

.PHONY: help install run demo clean check setup test logs

# Переменные
PYTHON := python3
PIP := pip3
VENV_DIR := venv
LOG_DIR := logs
APP_PORT := 8080

# Цвета для вывода
BOLD := \033[1m
GREEN := \033[32m
YELLOW := \033[33m
RED := \033[31m
RESET := \033[0m

# По умолчанию показываем справку
help:
	@echo "$(BOLD)QuantumStick Web Monitor - Управление проектом$(RESET)"
	@echo ""
	@echo "$(GREEN)Основные команды:$(RESET)"
	@echo "  make install    - Установка зависимостей и настройка окружения"
	@echo "  make run        - Запуск веб-мониторинга"
	@echo "  make demo       - Запуск в демо режиме (без реального устройства)"
	@echo "  make check      - Проверка системы и поиск устройств"
	@echo "  make clean      - Очистка временных файлов"
	@echo ""
	@echo "$(YELLOW)Дополнительные команды:$(RESET)"
	@echo "  make setup      - Только настройка виртуального окружения"
	@echo "  make test       - Запуск тестов (если есть)"
	@echo "  make logs       - Просмотр логов"
	@echo "  make stop       - Остановка всех процессов мониторинга"
	@echo ""
	@echo "$(GREEN)Примеры:$(RESET)"
	@echo "  make install && make run    - Полная установка и запуск"
	@echo "  make demo                   - Быстрый старт в демо режиме"
	@echo ""

# Проверка системных требований
check-system:
	@echo "$(BOLD)Проверка системных требований...$(RESET)"
	@command -v $(PYTHON) >/dev/null 2>&1 || { echo "$(RED)Ошибка: Python 3 не найден$(RESET)"; exit 1; }
	@command -v $(PIP) >/dev/null 2>&1 || { echo "$(RED)Ошибка: pip3 не найден$(RESET)"; exit 1; }
	@$(PYTHON) -c "import sys; assert sys.version_info >= (3, 8)" || { echo "$(RED)Ошибка: Требуется Python 3.8+$(RESET)"; exit 1; }
	@echo "$(GREEN)✓ Python $(shell $(PYTHON) --version | cut -d' ' -f2) найден$(RESET)"
	@command -v ethtool >/dev/null 2>&1 && echo "$(GREEN)✓ ethtool найден$(RESET)" || echo "$(YELLOW)⚠ ethtool не найден (рекомендуется установить)$(RESET)"
	@command -v ip >/dev/null 2>&1 && echo "$(GREEN)✓ ip найден$(RESET)" || echo "$(RED)✗ ip не найден (обязательно)$(RESET)"

# Настройка виртуального окружения
setup: check-system
	@echo "$(BOLD)Настройка виртуального окружения...$(RESET)"
	@if [ ! -d "$(VENV_DIR)" ]; then \
		echo "Создание виртуального окружения..."; \
		$(PYTHON) -m venv $(VENV_DIR); \
	fi
	@echo "$(GREEN)✓ Виртуальное окружение готово$(RESET)"

# Установка зависимостей
install-deps: setup
	@echo "$(BOLD)Установка зависимостей...$(RESET)"
	@. $(VENV_DIR)/bin/activate && \
		$(PIP) install --upgrade pip && \
		$(PIP) install -r requirements.txt
	@echo "$(GREEN)✓ Зависимости установлены$(RESET)"

# Создание необходимых директорий
create-dirs:
	@echo "$(BOLD)Создание рабочих директорий...$(RESET)"
	@mkdir -p $(LOG_DIR)
	@mkdir -p static
	@echo "$(GREEN)✓ Директории созданы$(RESET)"

# Полная установка
install: install-deps create-dirs
	@echo "$(GREEN)$(BOLD)✓ Установка завершена!$(RESET)"
	@echo "Для запуска используйте: make run"

# Проверка устройств TimeStick
check: check-system
	@echo "$(BOLD)Поиск устройств QuantumStick/TimeStick...$(RESET)"
	@echo "$(YELLOW)USB устройства ASIX:$(RESET)"
	@lsusb | grep -i "asix\|ax88" || echo "  Не найдены"
	@echo ""
	@echo "$(YELLOW)Сетевые интерфейсы с драйвером TimeStick:$(RESET)"
	@for iface in $$(ip link show | grep -E '^[0-9]+:' | awk -F': ' '{print $$2}' | grep -E '^eth|^enp' 2>/dev/null || true); do \
		if command -v ethtool >/dev/null 2>&1; then \
			driver=$$(ethtool -i $$iface 2>/dev/null | grep "driver:" | awk '{print $$2}' || echo "unknown"); \
			if echo "$$driver" | grep -q "ax88179\|ax88279"; then \
				echo "  $(GREEN)✓ $$iface ($$driver)$(RESET)"; \
			fi; \
		fi; \
	done
	@echo ""

# Запуск основного приложения
run: check-system
	@echo "$(BOLD)Запуск QuantumStick Web Monitor...$(RESET)"
	@if [ ! -d "$(VENV_DIR)" ]; then \
		echo "$(YELLOW)Виртуальное окружение не найдено. Запускаем установку...$(RESET)"; \
		make install; \
	fi
	@echo "$(GREEN)Веб-интерфейс будет доступен по адресу: http://localhost:$(APP_PORT)$(RESET)"
	@. $(VENV_DIR)/bin/activate && $(PYTHON) app.py

# Запуск в демо режиме
demo: check-system
	@echo "$(BOLD)Запуск QuantumStick Web Monitor в демо режиме...$(RESET)"
	@if [ ! -d "$(VENV_DIR)" ]; then \
		echo "$(YELLOW)Виртуальное окружение не найдено. Запускаем установку...$(RESET)"; \
		make install; \
	fi
	@echo "$(GREEN)Демо режим будет доступен по адресу: http://localhost:$(APP_PORT)$(RESET)"
	@echo "$(YELLOW)⚠️  Внимание: Это демонстрационный режим с имитацией данных!$(RESET)"
	@. $(VENV_DIR)/bin/activate && $(PYTHON) demo_mode.py

# Запуск с использованием скрипта (альтернативный способ)
run-script:
	@echo "$(BOLD)Запуск через start_monitor.sh...$(RESET)"
	@chmod +x start_monitor.sh
	@./start_monitor.sh

# Быстрый старт (установка + демо)
quickstart:
	@echo "$(BOLD)Быстрый старт QuantumStick Web Monitor$(RESET)"
	@make install
	@echo "$(GREEN)Запуск в демо режиме...$(RESET)"
	@make demo

# Остановка процессов
stop:
	@echo "$(BOLD)Остановка процессов мониторинга...$(RESET)"
	@pkill -f "python.*app.py" || true
	@pkill -f "python.*demo_mode.py" || true
	@echo "$(GREEN)✓ Процессы остановлены$(RESET)"

# Просмотр логов
logs:
	@echo "$(BOLD)Просмотр логов...$(RESET)"
	@if [ -d "$(LOG_DIR)" ] && [ "$$(ls -A $(LOG_DIR) 2>/dev/null)" ]; then \
		echo "$(GREEN)Доступные файлы логов:$(RESET)"; \
		ls -la $(LOG_DIR)/; \
		echo ""; \
		echo "$(YELLOW)Последние записи из новейшего лога:$(RESET)"; \
		tail -n 20 $$(ls -t $(LOG_DIR)/*.log 2>/dev/null | head -1) || echo "Логи пусты"; \
	else \
		echo "$(YELLOW)Файлы логов не найдены$(RESET)"; \
	fi

# Просмотр логов в реальном времени
logs-follow:
	@echo "$(BOLD)Просмотр логов в реальном времени...$(RESET)"
	@if [ -d "$(LOG_DIR)" ]; then \
		tail -f $(LOG_DIR)/*.log; \
	else \
		echo "$(YELLOW)Директория логов не найдена$(RESET)"; \
	fi

# Очистка временных файлов
clean:
	@echo "$(BOLD)Очистка временных файлов...$(RESET)"
	@find . -type f -name "*.pyc" -delete
	@find . -type d -name "__pycache__" -exec rm -rf {} + 2>/dev/null || true
	@find . -type f -name "*.log" -mtime +30 -delete 2>/dev/null || true
	@echo "$(GREEN)✓ Временные файлы очищены$(RESET)"

# Полная очистка (включая виртуальное окружение)
clean-all: clean
	@echo "$(BOLD)Полная очистка проекта...$(RESET)"
	@if [ -d "$(VENV_DIR)" ]; then \
		echo "Удаление виртуального окружения..."; \
		rm -rf $(VENV_DIR); \
	fi
	@if [ -d "$(LOG_DIR)" ]; then \
		echo "Очистка логов..."; \
		rm -rf $(LOG_DIR)/*.log 2>/dev/null || true; \
	fi
	@echo "$(GREEN)✓ Проект полностью очищен$(RESET)"

# Информация о проекте
info:
	@echo "$(BOLD)QuantumStick Web Monitor - Информация о проекте$(RESET)"
	@echo ""
	@echo "$(GREEN)Структура проекта:$(RESET)"
	@echo "  app.py              - Основное приложение"
	@echo "  demo_mode.py        - Демонстрационный режим"
	@echo "  templates/index.html - Веб-интерфейс"
	@echo "  requirements.txt    - Python зависимости"
	@echo "  start_monitor.sh    - Скрипт запуска"
	@echo ""
	@echo "$(GREEN)Состояние проекта:$(RESET)"
	@if [ -d "$(VENV_DIR)" ]; then \
		echo "  Виртуальное окружение: $(GREEN)✓ установлено$(RESET)"; \
	else \
		echo "  Виртуальное окружение: $(RED)✗ не установлено$(RESET)"; \
	fi
	@if [ -d "$(LOG_DIR)" ]; then \
		echo "  Директория логов: $(GREEN)✓ создана$(RESET)"; \
	else \
		echo "  Директория логов: $(YELLOW)⚠ не создана$(RESET)"; \
	fi
	@echo ""
	@echo "$(GREEN)Размер проекта:$(RESET)"
	@du -sh . 2>/dev/null || echo "  Не удалось определить"

# Установка системных зависимостей (Ubuntu/Debian)
install-system-ubuntu:
	@echo "$(BOLD)Установка системных зависимостей для Ubuntu/Debian...$(RESET)"
	@sudo apt update
	@sudo apt install -y python3 python3-pip python3-venv ethtool iproute2 usbutils
	@echo "$(GREEN)✓ Системные зависимости установлены$(RESET)"

# Установка системных зависимостей (CentOS/RHEL)
install-system-centos:
	@echo "$(BOLD)Установка системных зависимостей для CentOS/RHEL...$(RESET)"
	@sudo yum install -y python3 python3-pip ethtool iproute usbutils || \
	 sudo dnf install -y python3 python3-pip ethtool iproute usbutils
	@echo "$(GREEN)✓ Системные зависимости установлены$(RESET)"

# Создание systemd сервиса
install-service:
	@echo "$(BOLD)Создание systemd сервиса...$(RESET)"
	@echo "[Unit]" > quantum-monitor.service
	@echo "Description=QuantumStick Web Monitor" >> quantum-monitor.service
	@echo "After=network.target" >> quantum-monitor.service
	@echo "" >> quantum-monitor.service
	@echo "[Service]" >> quantum-monitor.service
	@echo "Type=simple" >> quantum-monitor.service
	@echo "User=$$(whoami)" >> quantum-monitor.service
	@echo "WorkingDirectory=$$(pwd)" >> quantum-monitor.service
	@echo "ExecStart=$$(pwd)/$(VENV_DIR)/bin/python $$(pwd)/app.py" >> quantum-monitor.service
	@echo "Restart=always" >> quantum-monitor.service
	@echo "RestartSec=10" >> quantum-monitor.service
	@echo "" >> quantum-monitor.service
	@echo "[Install]" >> quantum-monitor.service
	@echo "WantedBy=multi-user.target" >> quantum-monitor.service
	@echo "$(GREEN)Сервис создан: quantum-monitor.service$(RESET)"
	@echo "Для установки выполните:"
	@echo "  sudo cp quantum-monitor.service /etc/systemd/system/"
	@echo "  sudo systemctl daemon-reload"
	@echo "  sudo systemctl enable quantum-monitor"
	@echo "  sudo systemctl start quantum-monitor"

# Обновление проекта из git
update:
	@echo "$(BOLD)Обновление проекта...$(RESET)"
	@git pull origin main 2>/dev/null || echo "$(YELLOW)Git репозиторий не найден$(RESET)"
	@if [ -d "$(VENV_DIR)" ]; then \
		. $(VENV_DIR)/bin/activate && $(PIP) install --upgrade -r requirements.txt; \
	fi
	@echo "$(GREEN)✓ Проект обновлен$(RESET)"

# Создание backup конфигурации
backup:
	@echo "$(BOLD)Создание резервной копии...$(RESET)"
	@BACKUP_FILE="quantum_monitor_backup_$$(date +%Y%m%d_%H%M%S).tar.gz"; \
	tar -czf "$$BACKUP_FILE" --exclude=$(VENV_DIR) --exclude="*.log" .; \
	echo "$(GREEN)✓ Резервная копия создана: $$BACKUP_FILE$(RESET)"

# Проверка статуса
status:
	@echo "$(BOLD)Статус QuantumStick Web Monitor$(RESET)"
	@echo ""
	@if pgrep -f "python.*app.py" >/dev/null; then \
		echo "$(GREEN)✓ Основное приложение запущено (PID: $$(pgrep -f 'python.*app.py'))$(RESET)"; \
	else \
		echo "$(RED)✗ Основное приложение не запущено$(RESET)"; \
	fi
	@if pgrep -f "python.*demo_mode.py" >/dev/null; then \
		echo "$(GREEN)✓ Демо режим запущен (PID: $$(pgrep -f 'python.*demo_mode.py'))$(RESET)"; \
	else \
		echo "$(RED)✗ Демо режим не запущен$(RESET)"; \
	fi
	@echo ""
	@echo "$(GREEN)Сетевые порты:$(RESET)"
	@netstat -tlnp 2>/dev/null | grep ":$(APP_PORT)" || echo "  Порт $(APP_PORT) не используется"

# Тестирование соединения
test-connection:
	@echo "$(BOLD)Тестирование соединения с веб-интерфейсом...$(RESET)"
	@curl -s -o /dev/null -w "HTTP Status: %{http_code}\n" http://localhost:$(APP_PORT)/ || \
		echo "$(RED)Ошибка: Веб-сервер недоступен$(RESET)"

# Показать все доступные команды
list:
	@echo "$(BOLD)Все доступные команды:$(RESET)"
	@$(MAKE) -pRrq -f $(lastword $(MAKEFILE_LIST)) : 2>/dev/null | \
		awk -v RS= -F: '/^# File/,/^# Finished Make data base/ {if ($$1 !~ "^[#.]") {print $$1}}' | \
		sort | grep -E '^[a-zA-Z_-]+$$'