#!/bin/bash

# Скрипт для адаптации драйвера TimeStick к ядру Linux 6.11+
# Автор: AI Assistant
# Дата: $(date)

set -e

# Цвета для вывода
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Функция для вывода сообщений
log() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
    exit 1
}

warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# Проверка версии ядра
KERNEL_VERSION=$(uname -r)
KERNEL_MAJOR=$(echo $KERNEL_VERSION | cut -d. -f1)
KERNEL_MINOR=$(echo $KERNEL_VERSION | cut -d. -f2)

log "Текущая версия ядра: $KERNEL_VERSION"

# Проверка, нужен ли патч
if [ $KERNEL_MAJOR -lt 6 ] || ([ $KERNEL_MAJOR -eq 6 ] && [ $KERNEL_MINOR -lt 11 ]); then
    warning "Ваше ядро ($KERNEL_VERSION) не требует данного патча"
    read -p "Продолжить всё равно? (y/n): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 0
    fi
fi

# Проверка наличия директории с драйвером
if [ ! -d "TimeStick/DRV" ]; then
    error "Директория TimeStick/DRV не найдена. Запустите скрипт из корня репозитория."
fi

# Создание резервной копии
log "Создание резервной копии..."
BACKUP_DIR="TimeStick/DRV_backup_$(date +%Y%m%d_%H%M%S)"
cp -r TimeStick/DRV "$BACKUP_DIR"
log "Резервная копия создана в $BACKUP_DIR"

# Функция для патча файла
patch_file() {
    local file=$1
    local backup="${file}.orig"
    
    if [ ! -f "$file" ]; then
        warning "Файл $file не найден, пропускаем"
        return
    fi
    
    cp "$file" "$backup"
    log "Патчим файл $file..."
}

# Патч ax_main.h
patch_file "TimeStick/DRV/ax_main.h"
cat >> TimeStick/DRV/ax_main.h << 'EOF'

/* Compatibility for newer kernels - added by kernel 6.11 patch */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,11,0)
#include <linux/etherdevice.h>
#endif

/* For older kernels define eth_hw_addr_set stub */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,15,0)
static inline void eth_hw_addr_set(struct net_device *dev, const u8 *addr)
{
	memcpy(dev->dev_addr, addr, ETH_ALEN);
}
#endif
EOF

# Патч ax_main.c - замена ax_get_mac_address
log "Патчим функцию ax_get_mac_address в ax_main.c..."
sed -i.bak '
/static int ax_get_mac_address/,/^}/ {
    s/netdev->dev_addr/addr/g
    /struct net_device \*netdev = axdev->netdev;/a\
\	u8 addr[ETH_ALEN];
    /if (ax_check_ether_addr/,/dev_warn.*Found invalid MAC/ {
        s/if (ax_check_ether_addr.*/if (!is_valid_ether_addr(addr)) {/
        s/dev_warn.*Found invalid MAC.*/dev_warn(\&axdev->intf->dev, "Found invalid MAC address, using random");\
\		eth_hw_addr_random(netdev);\
\		memcpy(addr, netdev->dev_addr, ETH_ALEN);\
\	} else {\
\#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0)\
\		eth_hw_addr_set(netdev, addr);\
\#else\
\		memcpy(netdev->dev_addr, addr, ETH_ALEN);\
\#endif\
\	}/
    }
    /memcpy(netdev->perm_addr/i\
#if LINUX_VERSION_CODE < KERNEL_VERSION(6,11,0)
    /memcpy(netdev->perm_addr/a\
#endif
}
' TimeStick/DRV/ax_main.c

# Патч ax_set_mac_addr во всех файлах
for file in TimeStick/DRV/ax_main.c TimeStick/DRV/ax88179_178a.c TimeStick/DRV/ax88179a_772d.c; do
    if [ -f "$file" ]; then
        log "Патчим ax_set_mac_addr в $file..."
        sed -i '
        /memcpy(net->dev_addr, addr->sa_data, ETH_ALEN);/i\
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0)\
\	eth_hw_addr_set(net, addr->sa_data);\
#else
        /memcpy(net->dev_addr, addr->sa_data, ETH_ALEN);/a\
#endif
        ' "$file"
    fi
done

# Патч Makefile
log "Обновляем Makefile..."
if ! grep -q "KERNEL_VERSION :=" TimeStick/DRV/Makefile; then
    sed -i '3a\
\
# Kernel version detection\
KERNEL_VERSION := $(shell uname -r)\
KERNEL_MAJOR := $(shell echo $(KERNEL_VERSION) | cut -d. -f1)\
KERNEL_MINOR := $(shell echo $(KERNEL_VERSION) | cut -d. -f2)\
\
# For kernels 6.11 and above\
ifeq ($(shell [ $(KERNEL_MAJOR) -gt 6 -o \\( $(KERNEL_MAJOR) -eq 6 -a $(KERNEL_MINOR) -ge 11 \\) ] && echo 1),1)\
    EXTRA_CFLAGS += -DKERNEL_6_11_PLUS\
endif\
' TimeStick/DRV/Makefile
fi

# Компиляция драйвера
log "Компиляция драйвера..."
cd TimeStick/DRV
make clean
if make; then
    log "Драйвер успешно скомпилирован!"
    echo
    log "Для установки драйвера выполните:"
    echo "  cd TimeStick/DRV"
    echo "  sudo make install"
    echo "  sudo modprobe -r ax_usb_nic"
    echo "  sudo modprobe ax_usb_nic"
else
    error "Ошибка компиляции драйвера. Проверьте вывод выше."
fi

log "Патч успешно применён!"
log "Резервная копия сохранена в: $BACKUP_DIR"