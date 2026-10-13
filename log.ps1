# Запись лога с COM-порта платы в файл.
#   .\log.ps1                      # COM3, до Ctrl+C
#   .\log.ps1 -Tag gaz             # logs\2026-10-13_18-30-00_gaz.log
#   .\log.ps1 -Port COM5 -Seconds 60
# В файл пишется всё. В консоль выводятся только служебные строки, начинающиеся с '#'
# (heartbeat с rx/err), чтобы видеть состояние шины в реальном времени.
param(
    [string]$Port = 'COM3',
    [string]$Tag,
    [int]$Seconds = 0          # 0 — писать до Ctrl+C
)

$ErrorActionPreference = 'Stop'

$LogDir = Join-Path $PSScriptRoot 'logs'
New-Item -ItemType Directory -Force $LogDir | Out-Null

$name = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
if ($Tag) {
    # Оставляем в метке только безопасные для имени файла символы
    $safeTag = $Tag -replace '[^\w\-]', '_'
    $name += "_$safeTag"
}
$LogPath = Join-Path $LogDir "$name.log"

# Нативный USB: скорость ни на что не влияет, но ставим ту же, что в прошивке.
# DTR/RTS не трогаем, иначе USB-Serial/JTAG может перезагрузить чип.
$sp = New-Object System.IO.Ports.SerialPort $Port, 2000000
$sp.DtrEnable = $false
$sp.RtsEnable = $false
$sp.ReadTimeout = 200
$sp.Encoding = [System.Text.Encoding]::ASCII

$writer = New-Object System.IO.StreamWriter($LogPath, $false, (New-Object System.Text.UTF8Encoding $false))
$total = 0
$pending = ''

try {
    $sp.Open()
    Write-Host "Лог: $LogPath  (порт $Port, остановка — Ctrl+C)"
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $lastFlush = 0

    while ($Seconds -le 0 -or $sw.Elapsed.TotalSeconds -lt $Seconds) {
        $chunk = $sp.ReadExisting()
        if ($chunk.Length -eq 0) {
            Start-Sleep -Milliseconds 20
        } else {
            $writer.Write($chunk)
            # Режем на строки, чтобы найти служебные '#'; хвост без '\n' ждёт следующей порции
            $parts = ($pending + $chunk) -split "`n"
            $pending = $parts[-1]
            for ($i = 0; $i -lt $parts.Length - 1; $i++) {
                $line = $parts[$i].TrimEnd("`r")
                $total++
                if ($line.StartsWith('#')) { Write-Host $line }
            }
        }
        # Сбрасываем файл на диск раз в секунду, чтобы при обрыве терять минимум
        if ($sw.ElapsedMilliseconds - $lastFlush -ge 1000) {
            $writer.Flush()
            $lastFlush = $sw.ElapsedMilliseconds
        }
    }
}
finally {
    # Выполняется и при Ctrl+C
    $writer.Flush()
    $writer.Dispose()
    if ($sp.IsOpen) { $sp.Close() }
    $sp.Dispose()
    Write-Host "Записано строк: $total -> $LogPath"
}
