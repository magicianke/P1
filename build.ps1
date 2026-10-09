# Сборка и прошивка через arduino-cli из Arduino IDE 2.
#   .\build.ps1 compile
#   .\build.ps1 upload  [-Port COM5]
#   .\build.ps1 monitor [-Port COM5]
#   .\build.ps1 flash   [-Port COM5]   # upload + monitor
#   .\build.ps1 chipinfo [-Port COM5]  # узнать размер flash и наличие PSRAM
param(
    [Parameter(Position = 0)]
    [ValidateSet('compile', 'upload', 'monitor', 'flash', 'chipinfo')]
    [string]$Action = 'compile',
    [string]$Port
)

$ErrorActionPreference = 'Stop'

$Cli = "$env:LOCALAPPDATA\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
$Sketch = $PSScriptRoot
$BuildDir = Join-Path $PSScriptRoot 'build'

# Модуль ESP32-S3-DevKitC-1 N16R8: 16 МБ flash, 8 МБ OPI PSRAM (см. docs/hardware.md).
$BoardOptions = @(
    'FlashSize=16M'
    'PartitionScheme=app3M_fat9M_16MB'
    'PSRAM=opi'
    'USBMode=hwcdc'
    'CDCOnBoot=cdc'      # Serial = нативный USB (порт "USB" на плате)
    'CPUFreq=240'
) -join ','
$Fqbn = "esp32:esp32:esp32s3:$BoardOptions"

function Resolve-Port {
    if ($Port) { return $Port }
    $boards = & $Cli board list --format json | ConvertFrom-Json
    $ports = @($boards.detected_ports | Where-Object { $_.port.protocol -eq 'serial' } | ForEach-Object { $_.port.address })
    if ($ports.Count -eq 0) { throw 'Плата не найдена. Подключите её или укажите -Port COMx' }
    if ($ports.Count -gt 1) { Write-Host "Найдено несколько портов: $($ports -join ', '). Беру $($ports[0])" }
    return $ports[0]
}

switch ($Action) {
    'compile' {
        & $Cli compile --fqbn $Fqbn --build-path $BuildDir --warnings default $Sketch
    }
    'upload' {
        $p = Resolve-Port
        & $Cli compile --fqbn $Fqbn --build-path $BuildDir $Sketch
        if ($LASTEXITCODE -eq 0) { & $Cli upload --fqbn $Fqbn --input-dir $BuildDir -p $p $Sketch }
    }
    'monitor' {
        & $Cli monitor -p (Resolve-Port) -c baudrate=2000000
    }
    'flash' {
        & $PSCommandPath upload -Port $Port
        if ($LASTEXITCODE -eq 0) { & $PSCommandPath monitor -Port $Port }
    }
    'chipinfo' {
        $esptool = Get-ChildItem "$env:LOCALAPPDATA\Arduino15\packages\esp32\tools\esptool_py" -Recurse -Filter esptool.exe |
            Sort-Object { [version]$_.Directory.Name } | Select-Object -Last 1
        & $esptool.FullName --port (Resolve-Port) flash-id
    }
}
exit $LASTEXITCODE
