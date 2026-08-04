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

$modListPath = Join-Path $mewtatorMods 'modlist.txt'
$enabledMods = @(if (Test-Path -LiteralPath $modListPath) {
    @(Get-Content -LiteralPath $modListPath | ForEach-Object { $_.Trim() } | Where-Object { $_ })
} else {
    @()
})
if ($enabledMods -notcontains 'AutoCattery') {
    throw 'AutoCattery is installed but is not enabled in Mewtator modlist.txt.'
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
