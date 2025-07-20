# TimeStick Windows Driver

Драйвер для TimeStick USB 3.2 to 2.5G Ethernet адаптера под Windows 11

## Системные требования

- Windows 11 (22H2 или новее)
- Windows Driver Kit (WDK) для разработки
- Visual Studio 2022 Build Tools
- Права администратора для установки

## Структура проекта

```
windows_driver/
├── src/                    # Исходные файлы
│   ├── timestick_windows.h    # Заголовочный файл
│   ├── timestick_driver.c     # Основной драйвер
│   └── timestick_usermode.c   # Пользовательская библиотека
├── inf/                    # INF файлы для установки
│   └── timestick.inf          # Информация для установки
├── build/                  # Система сборки
│   └── Makefile              # Makefile для сборки
└── README_WINDOWS.md       # Данный файл
```

## Установка

### Метод 1: Использование готового пакета

1. Скачайте готовый установочный пакет
2. Запустите PowerShell или Command Prompt от имени администратора
3. Выполните команды:

```cmd
cd windows_driver\build
make package
make install
```

### Метод 2: Ручная установка

1. Подключите TimeStick устройство к компьютеру
2. Откройте Диспетчер устройств
3. Найдите TimeStick устройство (может отображаться как неизвестное устройство)
4. Правой кнопкой мыши → "Обновить драйвер"
5. Выберите "Найти драйверы на этом компьютере"
6. Укажите путь к папке `windows_driver\inf\`
7. Нажмите "Далее" и дождитесь установки

## Сборка из исходников

### Требования для сборки

1. Установите Windows Driver Kit (WDK):
   - Скачайте с сайта Microsoft
   - Установите вместе с Visual Studio 2022

2. Установите Visual Studio Build Tools 2022:
   - Включите компоненты C++ build tools
   - Убедитесь, что WDK совместим с версией Visual Studio

### Команды сборки

```cmd
# Переход в директорию сборки
cd windows_driver\build

# Просмотр доступных команд
make help

# Сборка всех компонентов
make all

# Сборка только драйвера
make driver

# Сборка пользовательской библиотеки
make usermode_lib

# Создание тестового приложения
make test_app

# Создание установочного пакета
make package

# Очистка
make clean
```

### Полная сборка WDK

Для полной сборки драйвера с WDK:

1. Откройте "Developer Command Prompt for VS 2022"
2. Выполните команды:

```cmd
cd windows_driver\build\output\driver
build -cZ
```

## Использование API

### C/C++ приложения

```c
#include "timestick_windows.h"

int main() {
    // Открытие устройства
    HANDLE hDevice = OpenTimeStickDevice();
    if (hDevice == INVALID_HANDLE_VALUE) {
        printf("Устройство не найдено\n");
        return 1;
    }
    
    // Получение информации об устройстве
    TIMESTICK_DEVICE_INFO deviceInfo;
    if (GetDeviceInfo(hDevice, &deviceInfo)) {
        printf("Драйвер: %s\n", deviceInfo.DriverSignature);
        printf("Версия: %s\n", deviceInfo.DeviceVersion);
        printf("Скорость: %u Mbps\n", deviceInfo.LinkSpeed);
    }
    
    // Получение статистики сети
    TIMESTICK_NETWORK_STATS stats;
    if (GetNetworkStats(hDevice, &stats)) {
        printf("RX пакеты: %llu\n", stats.RxPackets);
        printf("TX пакеты: %llu\n", stats.TxPackets);
    }
    
    // Управление PTP
    SetPtpControl(hDevice, TRUE);  // Включить PTP
    
    // Закрытие устройства
    CloseTimeStickDevice(hDevice);
    return 0;
}
```

### Компиляция приложения

```cmd
cl /I"path\to\timestick\headers" myapp.c timestick.lib setupapi.lib
```

## Web интерфейс с Windows поддержкой

Web интерфейс автоматически определяет платформу и предоставляет соответствующие функции:

### Для Windows:
- Автоматическое обнаружение TimeStick устройств
- Установка драйвера через web интерфейс
- Управление PTP через Windows API
- Мониторинг производительности

### Запуск web мониторинга:

```cmd
cd web_monitor
python app.py
```

Откройте браузер и перейдите по адресу: http://localhost:8080

## Устранение неполадок

### Драйвер не устанавливается

1. Проверьте цифровую подпись:
   ```cmd
   signtool verify /pa timestick.sys
   ```

2. Отключите проверку подписи (только для тестирования):
   ```cmd
   bcdedit /set testsigning on
   ```

3. Перезагрузите компьютер

### Устройство не обнаруживается

1. Проверьте в Диспетчере устройств:
   - Ищите устройства с VID_0B95
   - Проверьте статус устройства

2. Проверьте USB подключение:
   ```cmd
   Get-PnpDevice | Where-Object {$_.InstanceId -like "*VID_0B95*"}
   ```

### Проблемы с производительностью

1. Проверьте скорость USB порта:
   - Используйте USB 3.0/3.1/3.2 порт
   - Избегайте USB хабов

2. Обновите драйверы USB контроллера

3. Проверьте настройки электропитания:
   - Отключите "USB selective suspend"

## Отладка

### Включение отладочного вывода

1. В реестре создайте ключ:
   ```
   HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\TimeStickNet\Parameters
   ```

2. Добавьте DWORD значение:
   ```
   DebugLevel = 0xFFFFFFFF
   ```

3. Перезапустите службу или перезагрузите компьютер

### Просмотр логов

Логи драйвера можно просмотреть в Event Viewer:
- Windows Logs → System
- Applications and Services Logs → Microsoft → Windows → Kernel-PnP

## Удаление драйвера

```cmd
# Автоматическое удаление
make uninstall

# Ручное удаление
pnputil /delete-driver oem*.inf /uninstall /force
```

## Поддержка

При возникновении проблем:

1. Проверьте совместимость с Windows 11
2. Убедитесь в наличии прав администратора
3. Проверьте целостность системных файлов:
   ```cmd
   sfc /scannow
   ```

4. Создайте issue в репозитории с подробным описанием проблемы

## Версии

- v1.0.0 - Первая версия драйвера
- Поддержка AX88279/AX88179/AX88772D чипов
- Совместимость с Windows 11
- PTP поддержка
- Web интерфейс мониторинга

## Лицензия

Драйвер распространяется под лицензией, совместимой с исходным ASIX драйвером.

## Контакты

Для технической поддержки обращайтесь к разработчикам TimeStick проекта.