# Makefile для программ мониторинга TimeStick

CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread
LDFLAGS = -pthread -lncurses -lm -lrt
LDFLAGS_CONTROL = -lm -lrt

# Цели
TARGETS = timestick_monitor timestick_ptp_monitor timestick_control

# Правила
all: $(TARGETS)

timestick_monitor: timestick_monitor.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

timestick_ptp_monitor: timestick_ptp_monitor.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

timestick_control: timestick_control.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS_CONTROL)

# Установка
install: $(TARGETS)
	@echo "Installing TimeStick monitor programs..."
	@install -m 755 timestick_monitor /usr/local/bin/
	@install -m 755 timestick_ptp_monitor /usr/local/bin/
	@install -m 755 timestick_control /usr/local/bin/
	@echo "Installation complete!"

# Удаление
uninstall:
	@echo "Uninstalling TimeStick monitor programs..."
	@rm -f /usr/local/bin/timestick_monitor
	@rm -f /usr/local/bin/timestick_ptp_monitor
	@rm -f /usr/local/bin/timestick_control
	@echo "Uninstallation complete!"

# Очистка
clean:
	rm -f $(TARGETS) *.o *.log *.csv

# Помощь
help:
	@echo "TimeStick Monitor Build System"
	@echo "=============================="
	@echo "Targets:"
	@echo "  all      - Build all programs"
	@echo "  install  - Install programs to /usr/local/bin"
	@echo "  uninstall- Remove installed programs"
	@echo "  clean    - Clean build artifacts"
	@echo "  help     - Show this help"

.PHONY: all install uninstall clean help