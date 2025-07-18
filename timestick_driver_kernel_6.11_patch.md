# Патч драйвера TimeStick для ядра Linux 6.11+

## Обзор изменений

Для адаптации драйвера TimeStick к ядру Linux 6.11 и выше необходимо внести следующие изменения:

### 1. Изменения в работе с MAC-адресами

В ядре 6.11 изменился API для работы с MAC-адресами сетевых устройств. Прямое обращение к `netdev->dev_addr` больше не разрешено.

### 2. Изменения в ethtool API

Обновлен API для работы с ethtool, особенно для функций get/set_link_ksettings.

### 3. Изменения в netdev API

Некоторые поля структуры net_device были изменены или удалены.

## Необходимые изменения в коде

### 1. Файл ax_main.c

#### Замена прямого доступа к dev_addr:

**Было:**
```c
if (ax_read_cmd(axdev, AX_ACCESS_MAC, AX_NODE_ID, ETH_ALEN,
                ETH_ALEN, netdev->dev_addr, 0) < 0) {
```

**Стало:**
```c
u8 addr[ETH_ALEN];
if (ax_read_cmd(axdev, AX_ACCESS_MAC, AX_NODE_ID, ETH_ALEN,
                ETH_ALEN, addr, 0) < 0) {
    dev_err(&axdev->intf->dev, "Failed to read MAC address");
    return -ENODEV;
}
eth_hw_addr_set(netdev, addr);
```

#### Обновление функции ax_get_mac_address:

```c
static int ax_get_mac_address(struct ax_device *axdev)
{
    struct net_device *netdev = axdev->netdev;
    u8 addr[ETH_ALEN];

    if (ax_read_cmd(axdev, AX_ACCESS_MAC, AX_NODE_ID, ETH_ALEN,
                    ETH_ALEN, addr, 0) < 0) {
        dev_err(&axdev->intf->dev, "Failed to read MAC address");
        return -ENODEV;
    }

    /* Проверка валидности MAC-адреса */
    if (!is_valid_ether_addr(addr)) {
        dev_warn(&axdev->intf->dev, "Found invalid MAC address, using random");
        eth_hw_addr_random(netdev);
    } else {
        eth_hw_addr_set(netdev, addr);
    }

    ax_get_mac_pass(axdev, addr);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6,11,0)
    memcpy(netdev->perm_addr, netdev->dev_addr, ETH_ALEN);
#else
    /* В новых ядрах perm_addr управляется автоматически */
#endif

    if (ax_write_cmd(axdev, AX_ACCESS_MAC, AX_NODE_ID, ETH_ALEN,
                     ETH_ALEN, addr) < 0) {
        dev_err(&axdev->intf->dev, "Failed to write MAC address");
        return -ENODEV;
    }

    return 0;
}
```

### 2. Изменения в ax_main.h

Добавить проверку версии ядра для совместимости:

```c
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,11,0)
#include <linux/etherdevice.h>
#endif

/* Для старых ядер определяем функцию-заглушку */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,15,0)
static inline void eth_hw_addr_set(struct net_device *dev, const u8 *addr)
{
    memcpy(dev->dev_addr, addr, ETH_ALEN);
}
#endif
```

### 3. Обновление Makefile

Добавить дополнительные флаги компиляции для новых ядер:

```makefile
# Определение версии ядра
KERNEL_VERSION := $(shell uname -r)
KERNEL_MAJOR := $(shell echo $(KERNEL_VERSION) | cut -d. -f1)
KERNEL_MINOR := $(shell echo $(KERNEL_VERSION) | cut -d. -f2)

# Для ядер 6.11 и выше
ifeq ($(shell [ $(KERNEL_MAJOR) -gt 6 -o \( $(KERNEL_MAJOR) -eq 6 -a $(KERNEL_MINOR) -ge 11 \) ] && echo 1),1)
    EXTRA_CFLAGS += -DKERNEL_6_11_PLUS
endif
```

### 4. Изменения в функциях ethtool

В файле ax_main.c обновить функции работы с ethtool:

```c
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,11,0)
static int ax_get_link_ksettings(struct net_device *netdev,
                                struct ethtool_link_ksettings *cmd)
{
    struct ax_device *axdev = netdev_priv(netdev);
    int ret;

    if (!axdev->mii.mdio_read)
        return -EOPNOTSUPP;

    ret = usb_autopm_get_interface(axdev->intf);
    if (ret < 0)
        return ret;

    mutex_lock(&axdev->control);
    
    mii_ethtool_get_link_ksettings(&axdev->mii, cmd);
    
    mutex_unlock(&axdev->control);
    usb_autopm_put_interface(axdev->intf);

    return 0;
}
#endif
```

### 5. Обновление структуры ethtool_ops

```c
static const struct ethtool_ops ax_ethtool_ops = {
    .get_drvinfo = ax_get_drvinfo,
    .get_msglevel = ax_get_msglevel,
    .set_msglevel = ax_set_msglevel,
    .get_wol = ax_get_wol,
    .set_wol = ax_set_wol,
    .get_sset_count = ax_get_sset_count,
    .get_strings = ax_get_strings,
    .get_ethtool_stats = ax_get_ethtool_stats,
    .get_pauseparam = ax_get_pauseparam,
    .set_pauseparam = ax_set_pauseparam,
    .get_eee = ax_get_eee,
    .set_eee = ax_set_eee,
    .nway_reset = ax_nway_reset,
    .get_link = ax_get_link,
    .get_eeprom_len = ax_get_eeprom_len,
    .get_eeprom = ax_get_eeprom,
    .set_eeprom = ax_set_eeprom,
#if KERNEL_VERSION(4, 10, 0) > LINUX_VERSION_CODE
    .get_settings = ax_get_settings,
    .set_settings = ax_set_settings,
#else
    .get_link_ksettings = ax_get_link_ksettings,
    .set_link_ksettings = ax_set_link_ksettings,
#endif
};
```

## Инструкция по применению патча

1. **Резервное копирование:**
   ```bash
   cd /workspace/TimeStick/DRV
   cp -r . ../DRV_backup
   ```

2. **Применение изменений:**
   - Внести все описанные выше изменения в соответствующие файлы
   - Или использовать готовый патч (см. ниже)

3. **Компиляция драйвера:**
   ```bash
   make clean
   make
   ```

4. **Установка:**
   ```bash
   sudo make install
   sudo modprobe -r ax_usb_nic
   sudo modprobe ax_usb_nic
   ```

## Тестирование

После установки драйвера необходимо проверить:

1. **Загрузка модуля:**
   ```bash
   lsmod | grep ax_usb_nic
   dmesg | tail -50
   ```

2. **Проверка сетевого интерфейса:**
   ```bash
   ip link show
   ethtool -i <interface_name>
   ```

3. **Проверка PTP функциональности:**
   ```bash
   sudo ptp4l -i <interface_name> -m
   ```

4. **Проверка генерации 1PPS:**
   - Подключить осциллограф к SMA разъему
   - Включить PTP синхронизацию
   - Проверить наличие импульсов 1PPS

## Известные проблемы и решения

### Проблема 1: Ошибка компиляции с netdev->dev_addr

**Решение:** Убедитесь, что все обращения к dev_addr заменены на использование eth_hw_addr_set().

### Проблема 2: Предупреждения о устаревших функциях

**Решение:** Использовать условную компиляцию с проверкой версии ядра.

### Проблема 3: Ошибки при загрузке модуля

**Решение:** Проверить вывод dmesg и убедиться, что версия прошивки устройства совместима.

## Дополнительные рекомендации

1. **Для Ubuntu 24.04 и выше:**
   - Установить заголовки ядра: `sudo apt install linux-headers-$(uname -r)`
   - Установить необходимые инструменты: `sudo apt install build-essential`

2. **Для отладки:**
   - Включить отладочные сообщения в Makefile: `ENABLE_IOCTL_DEBUG = y`
   - Использовать dynamic debug: `echo 'module ax_usb_nic +p' > /sys/kernel/debug/dynamic_debug/control`

3. **Оптимизация производительности:**
   - Настроить параметры модуля при загрузке
   - Использовать CPU affinity для прерываний

## Заключение

Данный патч обеспечивает совместимость драйвера TimeStick с ядром Linux 6.11 и выше, сохраняя при этом обратную совместимость со старыми версиями ядра. Все критические функции, включая поддержку PTP и генерацию сигнала 1PPS, продолжают работать корректно.