[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$GameRoot
)

$ErrorActionPreference = 'Stop'
$resolvedGameRoot = (Resolve-Path -LiteralPath $GameRoot).Path
$runtimeRequired = @(
    'Mewgenics.exe',
    'version.dll',
    'chainloader.ini',
    'mods\AutoCattery.dll',
    'mods\AutoCattery\config\default_config.json',
    'mods\AutoCattery\config\config.schema.json',
    'mods\AutoCattery\localization\strings.json'
)

$missing = @()
foreach ($relativePath in $runtimeRequired) {
    $fullPath = Join-Path $resolvedGameRoot $relativePath
    if (-not (Test-Path -LiteralPath $fullPath)) {
        $missing += $relativePath
    }
}

if ($missing.Count -gt 0) {
    throw "Missing required install files: $($missing -join ', ')"
}

$mewtatorConfigPath = Join-Path $resolvedGameRoot 'Mewtator\config.json'
if (-not (Test-Path -LiteralPath $mewtatorConfigPath)) {
    throw 'Mewtator config.json is missing.'
}
$mewtatorConfig = Get-Content -LiteralPath $mewtatorConfigPath -Raw | ConvertFrom-Json
if (-not $mewtatorConfig.mod_folder) {
    throw 'Mewtator config.json does not define mod_folder.'
}
$mewtatorMods = [System.IO.Path]::GetFullPath([string]$mewtatorConfig.mod_folder)
$dataModRoot = Join-Path $mewtatorMods 'AutoCattery'
$dataRequired = @(
    'description.json',
    'data\text\combined.csv.append',
    'data\classes\classes.gon.merge',
    'data\classes\advanced_classes.gon.merge',
    'swfs\auto_cattery_house.swf',
    'swfs\swflist.gon.append'
)
foreach ($relativePath in $dataRequired) {
    if (-not (Test-Path -LiteralPath (Join-Path $dataModRoot $relativePath))) {
        $missing += "Mewtator data mod: $relativePath"
    }
}
if ($missing.Count -gt 0) {
    throw "Missing required install files: $($missing -join ', ')"
}

$rerollCount = 3
$userConfigPath = Join-Path $resolvedGameRoot 'mods\AutoCattery\config\user_config.json'
if (Test-Path -LiteralPath $userConfigPath) {
    $userConfig = Get-Content -LiteralPath $userConfigPath -Raw | ConvertFrom-Json
    if ($null -ne $userConfig.level_up.reroll_count) {
        $rerollCount = [int]$userConfig.level_up.reroll_count
    }
}
if ($rerollCount -lt 0 -or $rerollCount -gt 99) {
    throw 'Installed level-up reroll count is outside 0-99.'
}
$baseRerollData = Get-Content -LiteralPath `
    (Join-Path $dataModRoot 'data\classes\classes.gon.merge') -Raw
$advancedRerollData = Get-Content -LiteralPath `
    (Join-Path $dataModRoot 'data\classes\advanced_classes.gon.merge') -Raw
$expectedRerollLine = "AddLevelUpRerolls $rerollCount"
if (($baseRerollData | Select-String -Pattern ([regex]::Escape($expectedRerollLine)) -AllMatches).Matches.Count -ne 7 -or
    ($advancedRerollData | Select-String -Pattern ([regex]::Escape($expectedRerollLine)) -AllMatches).Matches.Count -ne 7) {
    throw 'Installed level-up reroll data does not match user_config.json for all 14 player classes.'
}

$modListPath = Join-Path $mewtatorMods 'modlist.txt'
$enabledMods = @(if (Test-Path -LiteralPath $modListPath) {
    @(Get-Content -LiteralPath $modListPath | ForEach-Object { $_.Trim() } | Where-Object { $_ })
} else {
    @()
})
if (@($enabledMods | Where-Object { $_ -ieq 'AutoCattery' }).Count -ne 1) {
    throw 'AutoCattery must appear exactly once in Mewtator modlist.txt.'
}
if ($enabledMods.Count -eq 0 -or $enabledMods[-1] -ine 'AutoCattery') {
    throw 'AutoCattery must be the last Mewtator data mod so its configured reroll value is not overwritten.'
}

$dllPath = Join-Path $resolvedGameRoot 'mods\AutoCattery.dll'
$stream = [System.IO.File]::OpenRead($dllPath)
$reader = [System.IO.BinaryReader]::new($stream)
try {
    $stream.Position = 0x3c
    $peOffset = $reader.ReadInt32()
    $stream.Position = $peOffset
    if ($reader.ReadUInt32() -ne 0x00004550) {
        throw 'AutoCattery.dll has an invalid PE signature.'
    }
    $machine = $reader.ReadUInt16()
    if ($machine -ne 0x8664) {
        throw 'AutoCattery.dll is not x64.'
    }
} finally {
    $reader.Dispose()
    $stream.Dispose()
}

Write-Host 'AutoCattery phase 03 runtime DLL, enabled Mewtator data mod, and DLL architecture are valid.'
Write-Host "Installed level-up rerolls verified for all 14 player classes: $rerollCount"
