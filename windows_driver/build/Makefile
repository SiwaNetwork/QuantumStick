#
# TimeStick Windows Driver Makefile
# Requires Windows Driver Kit (WDK) and Visual Studio Build Tools
#

# Переменные конфигурации
DRIVER_NAME = timestick
TARGET_OS = Win10
TARGET_ARCH = x64
BUILD_TYPE = Release

# Пути
WDK_PATH = C:\Program Files (x86)\Windows Kits\10
MSBUILD_PATH = C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin
SOURCE_DIR = ..\src
INF_DIR = ..\inf
OUTPUT_DIR = output

# Файлы
DRIVER_SOURCES = $(SOURCE_DIR)\timestick_driver.c
USERMODE_SOURCES = $(SOURCE_DIR)\timestick_usermode.c
HEADERS = $(SOURCE_DIR)\timestick_windows.h
INF_FILE = $(INF_DIR)\timestick.inf

# Цели сборки
all: driver usermode_lib test_app package

# Сборка драйвера
driver:
	@echo Building TimeStick driver for $(TARGET_OS) $(TARGET_ARCH)...
	@if not exist $(OUTPUT_DIR) mkdir $(OUTPUT_DIR)
	@if not exist $(OUTPUT_DIR)\driver mkdir $(OUTPUT_DIR)\driver
	
	# Создание SOURCES файла для WDK
	@echo TARGETNAME=$(DRIVER_NAME) > $(OUTPUT_DIR)\driver\SOURCES
	@echo TARGETTYPE=DRIVER >> $(OUTPUT_DIR)\driver\SOURCES
	@echo TARGETPATH=obj >> $(OUTPUT_DIR)\driver\SOURCES
	@echo INCLUDES=$(WDK_PATH)\Include\$(TARGET_OS)\km;$(WDK_PATH)\Include\$(TARGET_OS)\shared >> $(OUTPUT_DIR)\driver\SOURCES
	@echo SOURCES=$(DRIVER_SOURCES) >> $(OUTPUT_DIR)\driver\SOURCES
	@echo TARGETLIBS=$(WDK_PATH)\Lib\$(TARGET_OS)\km\$(TARGET_ARCH)\wdf01000.lib >> $(OUTPUT_DIR)\driver\SOURCES
	@echo TARGETLIBS=$(TARGETLIBS) $(WDK_PATH)\Lib\$(TARGET_OS)\km\$(TARGET_ARCH)\ntstrsafe.lib >> $(OUTPUT_DIR)\driver\SOURCES
	
	# Копирование исходных файлов
	copy $(DRIVER_SOURCES) $(OUTPUT_DIR)\driver\
	copy $(HEADERS) $(OUTPUT_DIR)\driver\
	
	@echo Driver build configuration created in $(OUTPUT_DIR)\driver

# Сборка пользовательской библиотеки
usermode_lib:
	@echo Building TimeStick user-mode library...
	@if not exist $(OUTPUT_DIR)\usermode mkdir $(OUTPUT_DIR)\usermode
	
	# Компиляция библиотеки
	cl /c /I$(SOURCE_DIR) $(USERMODE_SOURCES) /Fo$(OUTPUT_DIR)\usermode\timestick_usermode.obj
	lib /OUT:$(OUTPUT_DIR)\usermode\timestick.lib $(OUTPUT_DIR)\usermode\timestick_usermode.obj
	
	# Копирование заголовков
	copy $(HEADERS) $(OUTPUT_DIR)\usermode\
	
	@echo User-mode library built successfully

# Создание тестового приложения
test_app:
	@echo Creating test application...
	@if not exist $(OUTPUT_DIR)\test mkdir $(OUTPUT_DIR)\test
	
	# Создание тестового кода
	@echo #include "timestick_windows.h" > $(OUTPUT_DIR)\test\test_app.c
	@echo #include ^<stdio.h^> >> $(OUTPUT_DIR)\test\test_app.c
	@echo. >> $(OUTPUT_DIR)\test\test_app.c
	@echo int main() { >> $(OUTPUT_DIR)\test\test_app.c
	@echo     printf("TimeStick Test Application\n"); >> $(OUTPUT_DIR)\test\test_app.c
	@echo     if (IsTimeStickConnected()) { >> $(OUTPUT_DIR)\test\test_app.c
	@echo         printf("TimeStick device found!\n"); >> $(OUTPUT_DIR)\test\test_app.c
	@echo         CHAR signature[32]; >> $(OUTPUT_DIR)\test\test_app.c
	@echo         if (GetDriverSignature(signature, sizeof(signature))) { >> $(OUTPUT_DIR)\test\test_app.c
	@echo             printf("Driver: %%s\n", signature); >> $(OUTPUT_DIR)\test\test_app.c
	@echo         } >> $(OUTPUT_DIR)\test\test_app.c
	@echo     } else { >> $(OUTPUT_DIR)\test\test_app.c
	@echo         printf("TimeStick device not found\n"); >> $(OUTPUT_DIR)\test\test_app.c
	@echo     } >> $(OUTPUT_DIR)\test\test_app.c
	@echo     return 0; >> $(OUTPUT_DIR)\test\test_app.c
	@echo } >> $(OUTPUT_DIR)\test\test_app.c
	
	# Компиляция тестового приложения
	cl /I$(OUTPUT_DIR)\usermode $(OUTPUT_DIR)\test\test_app.c $(OUTPUT_DIR)\usermode\timestick.lib setupapi.lib /Fe$(OUTPUT_DIR)\test\timestick_test.exe
	
	@echo Test application created: $(OUTPUT_DIR)\test\timestick_test.exe

# Создание установочного пакета
package:
	@echo Creating installation package...
	@if not exist $(OUTPUT_DIR)\package mkdir $(OUTPUT_DIR)\package
	
	# Копирование файлов пакета
	copy $(INF_FILE) $(OUTPUT_DIR)\package\
	copy $(OUTPUT_DIR)\driver\$(DRIVER_NAME).sys $(OUTPUT_DIR)\package\ 2>nul || echo Driver binary not found - run full WDK build first
	copy $(OUTPUT_DIR)\usermode\timestick.lib $(OUTPUT_DIR)\package\
	copy $(OUTPUT_DIR)\usermode\timestick_windows.h $(OUTPUT_DIR)\package\
	copy $(OUTPUT_DIR)\test\timestick_test.exe $(OUTPUT_DIR)\package\
	
	# Создание README для установки
	@echo TimeStick Windows Driver Package > $(OUTPUT_DIR)\package\README.txt
	@echo ================================= >> $(OUTPUT_DIR)\package\README.txt
	@echo. >> $(OUTPUT_DIR)\package\README.txt
	@echo Installation Instructions: >> $(OUTPUT_DIR)\package\README.txt
	@echo 1. Right-click on timestick.inf and select "Install" >> $(OUTPUT_DIR)\package\README.txt
	@echo 2. Connect TimeStick device >> $(OUTPUT_DIR)\package\README.txt
	@echo 3. Run timestick_test.exe to verify installation >> $(OUTPUT_DIR)\package\README.txt
	@echo. >> $(OUTPUT_DIR)\package\README.txt
	@echo Files: >> $(OUTPUT_DIR)\package\README.txt
	@echo - timestick.inf: Driver installation file >> $(OUTPUT_DIR)\package\README.txt
	@echo - timestick.sys: Kernel driver (if built) >> $(OUTPUT_DIR)\package\README.txt
	@echo - timestick.lib: User-mode library >> $(OUTPUT_DIR)\package\README.txt
	@echo - timestick_windows.h: Header file >> $(OUTPUT_DIR)\package\README.txt
	@echo - timestick_test.exe: Test application >> $(OUTPUT_DIR)\package\README.txt
	
	@echo Installation package created in $(OUTPUT_DIR)\package

# Очистка
clean:
	@echo Cleaning build artifacts...
	@if exist $(OUTPUT_DIR) rmdir /s /q $(OUTPUT_DIR)
	@echo Clean completed

# Установка драйвера (требует прав администратора)
install:
	@echo Installing TimeStick driver...
	@if not exist $(OUTPUT_DIR)\package\timestick.inf (
		echo Installation package not found. Run 'make package' first.
		exit /b 1
	)
	
	# Установка INF файла
	pnputil /add-driver $(OUTPUT_DIR)\package\timestick.inf /install
	
	@echo Driver installation completed

# Удаление драйвера
uninstall:
	@echo Uninstalling TimeStick driver...
	pnputil /delete-driver oem*.inf /uninstall /force
	@echo Driver uninstallation completed

# Помощь
help:
	@echo TimeStick Windows Driver Build System
	@echo ===================================
	@echo.
	@echo Available targets:
	@echo   all         - Build everything (driver, library, test app, package)
	@echo   driver      - Prepare driver build configuration
	@echo   usermode_lib - Build user-mode library
	@echo   test_app    - Create test application
	@echo   package     - Create installation package
	@echo   clean       - Remove all build artifacts
	@echo   install     - Install driver (requires admin rights)
	@echo   uninstall   - Uninstall driver (requires admin rights)
	@echo   help        - Show this help
	@echo.
	@echo Requirements:
	@echo   - Windows Driver Kit (WDK)
	@echo   - Visual Studio Build Tools
	@echo   - Administrator rights for install/uninstall

.PHONY: all driver usermode_lib test_app package clean install uninstall help