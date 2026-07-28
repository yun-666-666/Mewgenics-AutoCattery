[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$GameRoot
)

$ErrorActionPreference = 'Stop'
$resolvedGameRoot = (Resolve-Path -LiteralPath $GameRoot).Path
$required = @(
    'Mewgenics.exe',
    'version.dll',
    'chainloader.ini',
    'mods\AutoCattery.dll',
    'mods\AutoCattery\config\default_config.json'
)

$missing = @()
foreach ($relativePath in $required) {
    $fullPath = Join-Path $resolvedGameRoot $relativePath
    if (-not (Test-Path -LiteralPath $fullPath)) {
        $missing += $relativePath
    }
}

if ($missing.Count -gt 0) {
    throw "Missing required install files: $($missing -join ', ')"
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

Write-Host 'AutoCattery phase 01 install structure and DLL architecture are valid.'
