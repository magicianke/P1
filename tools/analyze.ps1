# Анализ логов CAN (формат candump -L из прошивки: "(сек.мкс) can0 123#DEADBEEF").
#   .\tools\analyze.ps1 stats  logs\02_holostoy.log
#   .\tools\analyze.ps1 diff   logs\02_holostoy.log logs\03_gaz.log
#   .\tools\analyze.ps1 series logs\03_gaz.log -Id 0x140 [-Byte 2] [-Out gaz_140.csv]
#
# stats  — все ID: кадры, частота, DLC, по каждому байту min-max и число изменений.
# diff   — что меняется в логе с действием, но не меняется (или меняется уже) в спокойном.
# series — значения ID по времени в CSV (разделитель ';', время в мс от начала лога).
#
# В начале лога бывают старые кадры из буфера платы, а после перезагрузки время
# начинается заново. Поэтому лог режется на куски по разрывам времени > 0,5 с
# и анализируется самый длинный кусок.
param(
    [Parameter(Position = 0, Mandatory)]
    [ValidateSet('stats', 'diff', 'series')]
    [string]$Action,
    [Parameter(Position = 1, Mandatory)]
    [string]$Log,
    [Parameter(Position = 2)]
    [string]$Log2,
    [string]$Id,
    [int]$Byte = -1,
    [string]$Out
)

$ErrorActionPreference = 'Stop'

class Frame {
    [double]$T
    [int]$Id
    [bool]$Ext
    [int[]]$D
}

class IdStat {
    [int]$Id
    [bool]$Ext
    [int]$Count
    [double]$T0
    [double]$T1
    [int]$DlcMin = 99
    [int]$DlcMax = 0
    [int[]]$Min = @(255) * 8
    [int[]]$Max = @(0) * 8
    [int[]]$Chg = @(0) * 8      # сколько раз байт менялся
    [int[]]$Bits = @(0) * 8     # OR всех XOR соседних кадров: какие биты вообще менялись
    [int[]]$Prev = @(-1) * 8
    [object[]]$Vals             # множество значений каждого байта

    IdStat([int]$id, [bool]$ext) {
        $this.Id = $id
        $this.Ext = $ext
        $this.Vals = 0..7 | ForEach-Object { New-Object 'System.Collections.Generic.HashSet[int]' }
    }

    [double] Hz() {
        if ($this.Count -lt 2 -or $this.T1 -le $this.T0) { return 0 }
        return ($this.Count - 1) / ($this.T1 - $this.T0)
    }

    [string] IdText() {
        if ($this.Ext) { return '{0:X8}' -f $this.Id }
        return '{0:X3}' -f $this.Id
    }
}

$FrameRe = [regex]'^\((\d+)\.(\d+)\) can\d ([0-9A-Fa-f]+)#([0-9A-Fa-f]*)\s*$'
$AliveRe = [regex]'^# P1 alive (\d+)s rx:(\d+) fr/s:(\d+) miss:(\d+) err:(\d+) drop:(\d+)'

function Read-CanLog([string]$path) {
    if (-not (Test-Path $path)) { throw "Нет файла: $path" }
    $segments = New-Object System.Collections.Generic.List[object]
    $cur = New-Object System.Collections.Generic.List[Frame]
    $alive = New-Object System.Collections.Generic.List[object]
    $lastT = -1.0
    foreach ($line in [System.IO.File]::ReadLines((Resolve-Path $path))) {
        $m = $FrameRe.Match($line)
        if (-not $m.Success) {
            $a = $AliveRe.Match($line)
            if ($a.Success) {
                $alive.Add([pscustomobject]@{
                    Up = [int]$a.Groups[1].Value; Rx = [long]$a.Groups[2].Value; Fps = [int]$a.Groups[3].Value
                    Miss = [long]$a.Groups[4].Value; Err = [long]$a.Groups[5].Value; Drop = [long]$a.Groups[6].Value
                })
            }
            continue
        }
        $f = [Frame]::new()
        $f.T = [double]$m.Groups[1].Value + [double]$m.Groups[2].Value / 1e6
        $idHex = $m.Groups[3].Value
        $f.Id = [Convert]::ToInt32($idHex, 16)
        $f.Ext = $idHex.Length -gt 3
        $hex = $m.Groups[4].Value
        $n = [Math]::Min(8, [int]($hex.Length / 2))
        $f.D = New-Object int[] $n
        for ($i = 0; $i -lt $n; $i++) { $f.D[$i] = [Convert]::ToInt32($hex.Substring($i * 2, 2), 16) }
        # Разрыв или скачок времени назад (перезагрузка) — начинаем новый кусок
        if ($lastT -ge 0 -and ($f.T -lt $lastT -or $f.T - $lastT -gt 0.5)) {
            $segments.Add($cur)
            $cur = New-Object System.Collections.Generic.List[Frame]
        }
        $cur.Add($f)
        $lastT = $f.T
    }
    $segments.Add($cur)

    $best = $segments | Sort-Object Count -Descending | Select-Object -First 1
    $all = ($segments | Measure-Object Count -Sum).Sum
    [pscustomobject]@{
        Path = $path; Frames = $best; Dropped = $all - $best.Count; Segments = $segments.Count; Alive = $alive
    }
}

function Get-IdStats($frames) {
    $map = @{}
    foreach ($f in $frames) {
        $key = if ($f.Ext) { 'x' + $f.Id } else { [string]$f.Id }
        $s = $map[$key]
        if (-not $s) { $s = [IdStat]::new($f.Id, $f.Ext); $s.T0 = $f.T; $map[$key] = $s }
        $s.Count++
        $s.T1 = $f.T
        $n = $f.D.Length
        if ($n -lt $s.DlcMin) { $s.DlcMin = $n }
        if ($n -gt $s.DlcMax) { $s.DlcMax = $n }
        for ($i = 0; $i -lt $n; $i++) {
            $v = $f.D[$i]
            if ($v -lt $s.Min[$i]) { $s.Min[$i] = $v }
            if ($v -gt $s.Max[$i]) { $s.Max[$i] = $v }
            $p = $s.Prev[$i]
            if ($p -ge 0 -and $p -ne $v) { $s.Chg[$i]++; $s.Bits[$i] = $s.Bits[$i] -bor ($p -bxor $v) }
            $s.Prev[$i] = $v
            [void]$s.Vals[$i].Add($v)
        }
    }
    $map.Values | Sort-Object Ext, Id
}

function Write-LogSummary($log, $stats) {
    $fr = $log.Frames
    $dur = if ($fr.Count -gt 1) { $fr[$fr.Count - 1].T - $fr[0].T } else { 0 }
    Write-Host ("Лог {0}: кадров {1}, ID {2}, длительность {3:N1} с, в среднем {4:N0} кадр/с" -f `
        (Split-Path $log.Path -Leaf), $fr.Count, @($stats).Count, $dur, $(if ($dur -gt 0) { $fr.Count / $dur } else { 0 }))
    if ($log.Dropped -gt 0) {
        Write-Host ("  отброшено {0} кадров вне основного куска (старый буфер платы / перезагрузка), кусков: {1}" -f $log.Dropped, $log.Segments)
    }
    $al = $log.Alive
    if ($al.Count -gt 0) {
        # Счётчики после последней перезагрузки (rx не убывает)
        $first = 0
        for ($i = 1; $i -lt $al.Count; $i++) { if ($al[$i].Rx -lt $al[$i - 1].Rx) { $first = $i } }
        $a0 = $al[$first]; $a1 = $al[$al.Count - 1]
        Write-Host ("  плата: rx {0}, fr/s {1}, miss {2} (+{3}), err {4} (+{5}), drop {6} (+{7})" -f `
            $a1.Rx, $a1.Fps, $a1.Miss, ($a1.Miss - $a0.Miss), $a1.Err, ($a1.Err - $a0.Err), $a1.Drop, ($a1.Drop - $a0.Drop))
    }
}

# Компактная ячейка байта: "=AB" — не менялся, "00-FF:123" — диапазон и число изменений
function Format-ByteCell($s, $i) {
    if ($i -ge $s.DlcMax) { return '' }
    if ($s.Chg[$i] -eq 0) { return '={0:X2}' -f $s.Min[$i] }
    return '{0:X2}-{1:X2}:{2}' -f $s.Min[$i], $s.Max[$i], $s.Chg[$i]
}

function Show-Stats($log) {
    $stats = Get-IdStats $log.Frames
    Write-LogSummary $log $stats
    $rows = foreach ($s in $stats) {
        $o = [ordered]@{
            ID = $s.IdText(); Кадров = $s.Count; Гц = '{0:N1}' -f $s.Hz()
            DLC = $(if ($s.DlcMin -eq $s.DlcMax) { $s.DlcMax } else { '{0}-{1}' -f $s.DlcMin, $s.DlcMax })
        }
        for ($i = 0; $i -lt 8; $i++) { $o["B$i"] = Format-ByteCell $s $i }
        [pscustomobject]$o
    }
    $rows | Format-Table -Property * -AutoSize | Out-String -Width 300 | Write-Host
}

function Show-Diff($baseLog, $actLog) {
    $base = @{}; foreach ($s in (Get-IdStats $baseLog.Frames)) { $base["$($s.Ext)/$($s.Id)"] = $s }
    $actStats = Get-IdStats $actLog.Frames
    Write-LogSummary $baseLog $base.Values
    Write-LogSummary $actLog $actStats
    Write-Host ''

    $cands = New-Object System.Collections.Generic.List[object]
    $newIds = New-Object System.Collections.Generic.List[string]
    $act = @{}
    foreach ($a in $actStats) {
        $key = "$($a.Ext)/$($a.Id)"
        $act[$key] = $a
        $b = $base[$key]
        if (-not $b) { $newIds.Add(('{0} ({1:N1} Гц)' -f $a.IdText(), $a.Hz())); continue }
        for ($i = 0; $i -lt $a.DlcMax; $i++) {
            if ($a.Chg[$i] -eq 0) { continue }
            $bChg = if ($i -lt $b.DlcMax) { $b.Chg[$i] } else { 0 }
            $bBits = if ($i -lt $b.DlcMax) { $b.Bits[$i] } else { 0 }
            $newBits = $a.Bits[$i] -band (-bnot $bBits) -band 0xFF
            $bMin = $b.Min[$i]; $bMax = $b.Max[$i]
            $bRange = if ($i -lt $b.DlcMax) { $bMax - $bMin } else { 0 }
            $aRange = $a.Max[$i] - $a.Min[$i]
            # Новые значения вне диапазона спокойного лога
            $outside = 0
            foreach ($v in $a.Vals[$i]) { if ($v -lt $bMin -or $v -gt $bMax) { $outside++ } }

            $kind = $null
            if ($bChg -eq 0) { $kind = 'НОВОЕ' }                       # в спокойном не менялся вовсе
            elseif ($newBits -ne 0) { $kind = 'нов.биты' }              # менялись биты, которые в спокойном стояли
            elseif ($aRange -ge [Math]::Max(4, 2 * $bRange) -and $outside -gt 0) { $kind = 'шире' }  # диапазон заметно шире
            if (-not $kind) { continue }

            # Сигнал от действия обычно меняется реже, чем каждый кадр (счётчики и CRC меняются каждый кадр)
            $rate = $a.Chg[$i] / [Math]::Max(1, $a.Count - 1)
            $score = switch ($kind) { 'НОВОЕ' { 3 } 'нов.биты' { 2 } default { 1 } }
            if ($rate -gt 0.9) { $score -= 1.5 }

            $cands.Add([pscustomobject]@{
                Score = $score
                Тип = $kind
                ID = $a.IdText()
                Байт = $i
                'Биты(нов)' = $(if ($newBits) { '{0:X2}' -f $newBits } else { '' })
                Спокойно = $(if ($i -lt $b.DlcMax) { '{0:X2}-{1:X2}:{2}' -f $bMin, $bMax, $bChg } else { '-' })
                Действие = ('{0:X2}-{1:X2}:{2}' -f $a.Min[$i], $a.Max[$i], $a.Chg[$i])
                Знач = $a.Vals[$i].Count
                'Изм/кадр' = '{0:N2}' -f $rate
            })
        }
    }
    $gone = foreach ($k in $base.Keys) { if (-not $act.ContainsKey($k)) { $base[$k].IdText() } }

    if ($newIds.Count) { Write-Host ('ID только в логе с действием: ' + ($newIds -join ', ')) }
    if ($gone) { Write-Host ('ID только в спокойном логе: ' + (@($gone | Sort-Object) -join ', ')) }
    if ($cands.Count -eq 0) {
        Write-Host 'Кандидатов нет: в логе с действием не меняется ничего сверх спокойного.'
        return
    }
    Write-Host 'Кандидаты (сначала самые вероятные). Ячейки: min-max:изменений; Изм/кадр ~1 — счётчик/CRC.'
    $cands | Sort-Object @{ e = 'Score'; Descending = $true }, ID, Байт |
        Select-Object * -ExcludeProperty Score |
        Format-Table -AutoSize | Out-String -Width 300 | Write-Host
}

function Export-Series($log, [string]$idText, [int]$byteNo, [string]$outPath) {
    $want = [Convert]::ToInt32(($idText -replace '^0[xX]', ''), 16)
    $frames = @($log.Frames | Where-Object { $_.Id -eq $want })
    if ($frames.Count -eq 0) { throw "ID $idText в логе нет" }
    if (-not $outPath) {
        $sfx = if ($byteNo -ge 0) { "_b$byteNo" } else { '' }
        $outPath = [IO.Path]::ChangeExtension($log.Path, $null).TrimEnd('.') + ('_{0:X3}{1}.csv' -f $want, $sfx)
    }
    $t0 = $log.Frames[0].T
    $sb = New-Object System.Text.StringBuilder
    if ($byteNo -ge 0) { [void]$sb.AppendLine("t_ms;b$byteNo") }
    else { [void]$sb.AppendLine('t_ms;' + ((0..7 | ForEach-Object { "b$_" }) -join ';')) }
    foreach ($f in $frames) {
        $ms = [long][Math]::Round(($f.T - $t0) * 1000)
        if ($byteNo -ge 0) {
            $v = if ($byteNo -lt $f.D.Length) { $f.D[$byteNo] } else { '' }
            [void]$sb.AppendLine("$ms;$v")
        } else {
            $cells = 0..7 | ForEach-Object { if ($_ -lt $f.D.Length) { $f.D[$_] } else { '' } }
            [void]$sb.AppendLine("$ms;" + ($cells -join ';'))
        }
    }
    [IO.File]::WriteAllText($outPath, $sb.ToString(), [Text.Encoding]::ASCII)
    Write-Host ("Записано {0} точек -> {1}" -f $frames.Count, $outPath)
}

switch ($Action) {
    'stats' { Show-Stats (Read-CanLog $Log) }
    'diff' {
        if (-not $Log2) { throw 'diff: нужны два лога — спокойный и с действием' }
        Show-Diff (Read-CanLog $Log) (Read-CanLog $Log2)
    }
    'series' {
        if (-not $Id) { throw 'series: укажите -Id, например -Id 0x140' }
        Export-Series (Read-CanLog $Log) $Id $Byte $Out
    }
}
