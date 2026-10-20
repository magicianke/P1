# Прогон записанного лога через разбор сигналов приборки: что показал бы экран.
#   .\tools\replay.ps1 logs\2026-10-14_18-49-50_05_selector.log
#   .\tools\replay.ps1 logs\...log -StepMs 200        # строка не реже раза в 200 мс
#   .\tools\replay.ps1 logs\...log -StepMs 0          # только изменения селектора/тормоза/двери/«--»
#
# ID, байты, маски, пороги и kStaleMs читаются из src/can/signals.h (константы kXxx),
# поэтому здесь продублированы только сами формулы — по одной строке на сигнал.
# Компилятора C++ для ПК нет, так что тот же код, что в прошивке, здесь не запустить.
#
# Как в analyze.ps1: лог режется по разрывам времени > 0,5 с, берётся самый длинный кусок
# (в начале бывают старые кадры из буфера платы). Время — секунды от начала куска.
# Строки с '*' — изменился показ селектора, тормоза, двери или чего-то «--».
param(
    [Parameter(Position = 0, Mandatory)]
    [string]$Log,
    [int]$StepMs = 1000,
    [string]$Signals = (Join-Path $PSScriptRoot '..\src\can\signals.h')
)

$ErrorActionPreference = 'Stop'

# --- Константы из signals.h ---
$K = @{}
foreach ($line in Get-Content $Signals -Encoding UTF8) {
    if ($line -match '^\s*constexpr\s+[\w:]+\s+(k\w+)\s*=\s*(0x[0-9A-Fa-f]+|-?\d+)u?\s*;') {
        $K[$Matches[1]] = [Convert]::ToInt64($Matches[2], $(if ($Matches[2] -like '0x*') { 16 } else { 10 }))
    }
}
$need = 'kRpmId', 'kRpmLoByte', 'kRpmHiByte', 'kCoolantId', 'kCoolantByte', 'kCoolantOffset',
    'kSelectorId', 'kSelectorByte', 'kSelectorCodeP', 'kSelectorCodeR', 'kSelectorCodeN', 'kSelectorCodeD',
    'kSelectorCodeBetween', 'kBrakeId', 'kBrakeByte', 'kBrakeMask', 'kDoorId', 'kDoorByte', 'kDoorClosedMask',
    'kStaleMs', 'kCoolantColdC', 'kCoolantHotC'
$missing = $need | Where-Object { -not $K.ContainsKey($_) }
if ($missing) { throw "В $Signals не найдены константы: $($missing -join ', ')" }

$selCodes = @{
    [int]$K.kSelectorCodeP = 'P'; [int]$K.kSelectorCodeR = 'R'
    [int]$K.kSelectorCodeN = 'N'; [int]$K.kSelectorCodeD = 'D'
}
$staleS = $K.kStaleMs / 1000.0

# --- Чтение лога, выбор самого длинного куска ---
$rx = [regex]'^\((\d+)\.(\d+)\)\s+\S+\s+([0-9A-Fa-f]{3}|[0-9A-Fa-f]{8})#([0-9A-Fa-f]*)'
$chunks = New-Object System.Collections.Generic.List[object]
$cur = New-Object System.Collections.Generic.List[object]
$prevT = $null
foreach ($line in [System.IO.File]::ReadLines((Resolve-Path $Log))) {
    $m = $rx.Match($line)
    if (-not $m.Success) { continue }
    $t = [double]$m.Groups[1].Value + [double]("0." + $m.Groups[2].Value)
    if ($null -ne $prevT -and ($t -lt $prevT -or $t - $prevT -gt 0.5)) {
        $chunks.Add($cur); $cur = New-Object System.Collections.Generic.List[object]
    }
    $prevT = $t
    if ($m.Groups[3].Value.Length -ne 3) { continue }   # только 11-битные, как в прошивке
    $hex = $m.Groups[4].Value
    $d = New-Object int[] ($hex.Length / 2)
    for ($i = 0; $i -lt $d.Length; $i++) { $d[$i] = [Convert]::ToInt32($hex.Substring($i * 2, 2), 16) }
    $cur.Add([pscustomobject]@{ T = $t; Id = [Convert]::ToInt32($m.Groups[3].Value, 16); D = $d })
}
$chunks.Add($cur)
$frames = $chunks | Sort-Object Count -Descending | Select-Object -First 1
if (-not $frames -or $frames.Count -eq 0) { throw "В $Log нет кадров" }
$t0 = $frames[0].T
Write-Host ("Лог: {0}, кусков: {1}, взят самый длинный: {2} кадров, {3:N1} с" -f
    (Split-Path $Log -Leaf), $chunks.Count, $frames.Count, ($frames[-1].T - $t0))

# --- Состояние как sig::DashState: значение и время последнего кадра ---
$S = @{}
foreach ($n in 'rpm', 'coolant', 'selector', 'brake', 'door') { $S[$n] = @{ V = $null; T = $null } }
$S.selector.V = '?'   # Selector::Unknown

function Put($name, $v, $t) { $S[$name].V = $v; $S[$name].T = $t }
function Fresh($name, $t) { $null -ne $S[$name].T -and ($t - $S[$name].T) -le $staleS }

# Формулы — те же, что decode() в signals.h
function Decode($f) {
    $d = $f.D
    switch ($f.Id) {
        $K.kRpmId { if ($d.Length -gt $K.kRpmHiByte) { Put 'rpm' ($d[$K.kRpmLoByte] -bor ($d[$K.kRpmHiByte] -shl 8)) $f.T } }
        $K.kCoolantId { if ($d.Length -gt $K.kCoolantByte) { Put 'coolant' ($d[$K.kCoolantByte] - $K.kCoolantOffset) $f.T } }
        $K.kSelectorId {
            if ($d.Length -gt $K.kSelectorByte) {
                $code = $d[$K.kSelectorByte]
                $sel = if ($code -eq $K.kSelectorCodeBetween) { $S.selector.V }      # 0x08 — оставить последнее
                       elseif ($selCodes.ContainsKey($code)) { $selCodes[$code] } else { '?' }
                Put 'selector' $sel $f.T
            }
        }
        $K.kBrakeId { if ($d.Length -gt $K.kBrakeByte) { Put 'brake' (($d[$K.kBrakeByte] -band $K.kBrakeMask) -ne 0) $f.T } }
        $K.kDoorId { if ($d.Length -gt $K.kDoorByte) { Put 'door' (($d[$K.kDoorByte] -band $K.kDoorClosedMask) -eq 0) $f.T } }
    }
}

# Что на экране (как в dash_screen.cpp)
function Screen($t) {
    $rpm = if (Fresh 'rpm' $t) { "$($S.rpm.V)" } else { '--' }
    $ozh = '--'
    if (Fresh 'coolant' $t) {
        $c = $S.coolant.V
        $col = if ($c -gt $K.kCoolantHotC) { 'красный' } elseif ($c -lt $K.kCoolantColdC) { 'синий' } else { 'белый' }
        $ozh = "$c ($col)"
    }
    $sel = if (Fresh 'selector' $t) { $S.selector.V } else { '--' }
    $brk = if (-not (Fresh 'brake' $t)) { '--' } elseif ($S.brake.V) { 'НАЖАТ' } else { 'отпущен' }
    $door = if (-not (Fresh 'door' $t)) { '--' } elseif ($S.door.V) { 'ОТКРЫТА' } else { 'закрыта' }
    [pscustomobject]@{ Rpm = $rpm; Ozh = $ozh; Sel = $sel; Brk = $brk; Door = $door
        # Показ, изменение которого печатается сразу (обороты и ОЖ — только по шагу)
        Key = "$sel|$brk|$door|$($rpm -eq '--')|$($ozh -eq '--')" }
}

$fmt = '{0,1} {1,8:N2}  {2,7}  {3,-14} {4,-3} {5,-8} {6}'
Write-Host ($fmt -f ' ', 'время, с', 'обороты', 'ОЖ, °C', 'сел', 'тормоз', 'дверь')
$lastKey = $null
$nextStep = 0.0
$rows = 0
foreach ($f in $frames) {
    Decode $f
    $t = $f.T - $t0
    $scr = Screen $f.T
    $event = $scr.Key -ne $lastKey
    $step = $StepMs -gt 0 -and $t -ge $nextStep
    if ($event -or $step) {
        Write-Host ($fmt -f $(if ($event -and $null -ne $lastKey) { '*' } else { ' ' }), $t, $scr.Rpm, $scr.Ozh, $scr.Sel, $scr.Brk, $scr.Door)
        $rows++
        $lastKey = $scr.Key
        if ($StepMs -gt 0) { while ($nextStep -le $t) { $nextStep += $StepMs / 1000.0 } }
    }
}
# Через kStaleMs после последнего кадра всё должно стать «--»
$tEnd = $frames[-1].T + $staleS + 0.001
$scr = Screen $tEnd
Write-Host ($fmt -f '*', ($tEnd - $t0), $scr.Rpm, $scr.Ozh, $scr.Sel, $scr.Brk, $scr.Door) '(кадров больше нет)'
