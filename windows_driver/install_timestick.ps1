# TimeStick Windows Driver Installation Script
# Автоматическая установка драйвера TimeStick для Windows 11

param(
    [Parameter(Mandatory=$false)]
    [string]$DriverPath = ".\inf\timestick.inf",
    
    [Parameter(Mandatory=$false)]
    [switch]$Force,
    
    [Parameter(Mandatory=$false)]
    [switch]$TestMode,
    
    [Parameter(Mandatory=$false)]
    [switch]$Uninstall
)

# Проверка прав администратора
function Test-Administrator {
    $currentUser = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($currentUser)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

# Логирование
function Write-Log {
    param($Message, $Level = "INFO")
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $logMessage = "[$timestamp] [$Level] $Message"
    Write-Host $logMessage
    Add-Content -Path "timestick_install.log" -Value $logMessage
}

# Проверка совместимости системы
function Test-SystemCompatibility {
    Write-Log "Проверка совместимости системы..."
    
    $osVersion = [System.Environment]::OSVersion.Version
    $osName = (Get-WmiObject -Class Win32_OperatingSystem).Caption
    
    Write-Log "ОС: $osName"
    Write-Log "Версия: $($osVersion.ToString())"
    
    # Проверка Windows 11
    if ($osVersion.Major -lt 10 -or ($osVersion.Major -eq 10 -and $osVersion.Build -lt 22000)) {
        Write-Log "ВНИМАНИЕ: Рекомендуется Windows 11 (Build 22000+)" "WARNING"
        return $false
    }
    
    Write-Log "Система совместима" "SUCCESS"
    return $true
}

# Поиск TimeStick устройств
function Find-TimeStickDevices {
    Write-Log "Поиск TimeStick устройств..."
    
    $devices = Get-PnpDevice | Where-Object {
        $_.InstanceId -like "*VID_0B95*" -and 
        ($_.InstanceId -like "*PID_1790*" -or 
         $_.InstanceId -like "*PID_1791*" -or 
         $_.InstanceId -like "*PID_1792*")
    }
    
    if ($devices) {
        Write-Log "Найдено устройств TimeStick: $($devices.Count)" "SUCCESS"
        foreach ($device in $devices) {
            Write-Log "  - $($device.FriendlyName) ($($device.Status))"
        }
    } else {
        Write-Log "TimeStick устройства не найдены" "WARNING"
    }
    
    return $devices
}

# Включение тестового режима подписи
function Enable-TestSigning {
    if ($TestMode) {
        Write-Log "Включение тестового режима подписи..."
        try {
            $result = Start-Process -FilePath "bcdedit" -ArgumentList "/set testsigning on" -Wait -PassThru -Verb RunAs
            if ($result.ExitCode -eq 0) {
                Write-Log "Тестовый режим подписи включен. Требуется перезагрузка." "SUCCESS"
                return $true
            } else {
                Write-Log "Ошибка включения тестового режима подписи" "ERROR"
                return $false
            }
        } catch {
            Write-Log "Исключение при включении тестового режима: $($_.Exception.Message)" "ERROR"
            return $false
        }
    }
    return $true
}

# Установка драйвера
function Install-TimeStickDriver {
    param($InfPath)
    
    Write-Log "Установка драйвера из: $InfPath"
    
    if (-not (Test-Path $InfPath)) {
        Write-Log "INF файл не найден: $InfPath" "ERROR"
        return $false
    }
    
    try {
        # Использование pnputil для установки
        $result = Start-Process -FilePath "pnputil" -ArgumentList "/add-driver", $InfPath, "/install" -Wait -PassThru -Verb RunAs -RedirectStandardOutput "pnputil_output.txt" -RedirectStandardError "pnputil_error.txt"
        
        $output = Get-Content "pnputil_output.txt" -ErrorAction SilentlyContinue
        $error = Get-Content "pnputil_error.txt" -ErrorAction SilentlyContinue
        
        if ($output) { Write-Log "PnPUtil Output: $($output -join ' ')" }
        if ($error) { Write-Log "PnPUtil Error: $($error -join ' ')" "ERROR" }
        
        if ($result.ExitCode -eq 0) {
            Write-Log "Драйвер успешно установлен" "SUCCESS"
            return $true
        } else {
            Write-Log "Ошибка установки драйвера. Код выхода: $($result.ExitCode)" "ERROR"
            return $false
        }
    } catch {
        Write-Log "Исключение при установке драйвера: $($_.Exception.Message)" "ERROR"
        return $false
    } finally {
        # Очистка временных файлов
        Remove-Item "pnputil_output.txt" -ErrorAction SilentlyContinue
        Remove-Item "pnputil_error.txt" -ErrorAction SilentlyContinue
    }
}

# Удаление драйвера
function Uninstall-TimeStickDriver {
    Write-Log "Удаление драйвера TimeStick..."
    
    try {
        # Поиск установленных OEM драйверов
        $drivers = pnputil /enum-drivers | Select-String -Pattern "oem\d+\.inf" | ForEach-Object {
            $oemFile = $_.Matches[0].Value
            $driverInfo = pnputil /enum-drivers /files | Select-String -A 10 -Pattern $oemFile
            if ($driverInfo -match "timestick|TimeStick") {
                return $oemFile
            }
        }
        
        if ($drivers) {
            foreach ($driver in $drivers) {
                Write-Log "Удаление драйвера: $driver"
                $result = Start-Process -FilePath "pnputil" -ArgumentList "/delete-driver", $driver, "/uninstall", "/force" -Wait -PassThru -Verb RunAs
                
                if ($result.ExitCode -eq 0) {
                    Write-Log "Драйвер $driver удален" "SUCCESS"
                } else {
                    Write-Log "Ошибка удаления драйвера $driver" "ERROR"
                }
            }
        } else {
            Write-Log "Установленные драйверы TimeStick не найдены" "WARNING"
        }
        
        return $true
    } catch {
        Write-Log "Исключение при удалении драйвера: $($_.Exception.Message)" "ERROR"
        return $false
    }
}
}

# Проверка статуса установки
function Test-DriverInstallation {
    Write-Log "Проверка статуса установки драйвера..."
    
    $devices = Find-TimeStickDevices
    $workingDevices = $devices | Where-Object { $_.Status -eq "OK" }
    
    if ($workingDevices) {
        Write-Log "Успешно работающих устройств: $($workingDevices.Count)" "SUCCESS"
        return $true
    } else {
        Write-Log "Рабочие устройства не найдены" "WARNING"
        return $false
    }
}

# Создание отчета
function Create-InstallationReport {
    $report = @{
        Timestamp = Get-Date
        System = @{
            OS = (Get-WmiObject -Class Win32_OperatingSystem).Caption
            Version = [System.Environment]::OSVersion.Version.ToString()
            Architecture = [System.Environment]::Is64BitOperatingSystem
        }
        Devices = @()
        Success = $false
    }
    
    $devices = Find-TimeStickDevices
    foreach ($device in $devices) {
        $report.Devices += @{
            Name = $device.FriendlyName
            Status = $device.Status
            InstanceId = $device.InstanceId
        }
    }
    
    $report.Success = ($devices | Where-Object { $_.Status -eq "OK" }).Count -gt 0
    
    $reportJson = $report | ConvertTo-Json -Depth 3
    Set-Content -Path "timestick_report.json" -Value $reportJson
    
    Write-Log "Отчет сохранен в timestick_report.json"
    return $report
}

# Главная функция
function Main {
    Write-Log "=== TimeStick Driver Installation Script ===" "INFO"
    Write-Log "Версия: 1.0.0"
    Write-Log "Дата: $(Get-Date)"
    
    # Проверка прав администратора
    if (-not (Test-Administrator)) {
        Write-Log "Требуются права администратора. Перезапустите скрипт от имени администратора." "ERROR"
        exit 1
    }
    
    # Проверка совместимости
    if (-not (Test-SystemCompatibility)) {
        if (-not $Force) {
            Write-Log "Система может быть несовместима. Используйте -Force для принудительной установки." "ERROR"
            exit 1
        }
    }
    
    # Поиск устройств
    $initialDevices = Find-TimeStickDevices
    
    try {
        if ($Uninstall) {
            # Удаление драйвера
            $success = Uninstall-TimeStickDriver
            
            if ($success) {
                Write-Log "Драйвер успешно удален" "SUCCESS"
            } else {
                Write-Log "Ошибка удаления драйвера" "ERROR"
                exit 1
            }
        } else {
            # Установка драйвера
            
            # Включение тестового режима если необходимо
            if (-not (Enable-TestSigning)) {
                Write-Log "Не удалось включить тестовый режим подписи" "WARNING"
            }
            
            # Установка
            $success = Install-TimeStickDriver -InfPath $DriverPath
            
            if ($success) {
                Write-Log "Драйвер успешно установлен" "SUCCESS"
                
                # Небольшая пауза для инициализации устройств
                Start-Sleep -Seconds 3
                
                # Проверка статуса
                if (Test-DriverInstallation) {
                    Write-Log "Установка завершена успешно" "SUCCESS"
                } else {
                    Write-Log "Драйвер установлен, но устройства могут требовать переподключения" "WARNING"
                }
            } else {
                Write-Log "Ошибка установки драйвера" "ERROR"
                exit 1
            }
        }
        
        # Создание отчета
        $report = Create-InstallationReport
        
        Write-Log "=== Установка завершена ===" "SUCCESS"
        
    } catch {
        Write-Log "Критическая ошибка: $($_.Exception.Message)" "ERROR"
        exit 1
    }
}
}

# Запуск основной функции
Main